// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GLTest/RenderTest.h>

#include <tlRender/GL/OffscreenRender.h>
#include <tlRender/GL/Render.h>

#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/CPUColor.h>
#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Transition.h>

#include <ftk/GL/Init.h>
#include <ftk/GL/GL.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Texture.h>
#include <ftk/GL/Window.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>
#include <ftk/Core/Mesh.h>
#include <ftk/Core/Path.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Memory.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>

#include <array>
#include <filesystem>
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    namespace gl_test
    {
        RenderTest::RenderTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "tl::gl_test::RenderTest")
        {}

        RenderTest::~RenderTest()
        {}

        std::shared_ptr<RenderTest> RenderTest::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<RenderTest>(new RenderTest(context));
        }

        namespace
        {
            const ftk::Size2I imageSize(64, 32);

            std::shared_ptr<ftk::gl::Window> createWindow(
                const std::shared_ptr<ftk::Context>& context)
            {
                return ftk::gl::Window::create(
                    context,
                    "tl::gl_test::RenderTest",
                    ftk::Size2I(100, 100),
                    static_cast<int>(ftk::gl::WindowOptions::MakeCurrent));
            }

            //! An image filled with one value, so that a comparison of two of
            //! them has something to find.
            std::shared_ptr<ftk::Image> createImage(uint8_t value)
            {
                auto out = ftk::Image::create(imageSize, ftk::ImageType::RGBA_U8);
                uint8_t* p = out->getData();
                std::fill(p, p + out->getByteCount(), value);
                return out;
            }

            VideoFrame createFrame(uint8_t value)
            {
                VideoLayer layer;
                layer.image = createImage(value);
                VideoFrame out;
                out.size = imageSize;
                out.layers.push_back(layer);
                return out;
            }
        }

        void RenderTest::run()
        {
            _compare();
            _offscreenRender();
            _dissolve();
            _display();
            _background();
            _foreground();
            _prims();
            _oneToOne();
            _yuvLevels();
            _color();
            _cpuColor();
        }

        //! Video frames rendered to an image with no window to show them:
        //! one frame at its own size, two side by side, and two fitted into
        //! a size of their own.
        void RenderTest::_offscreenRender()
        {
            // Opaque, so the color read back is the color drawn.
            const auto opaqueFrame = [](uint8_t value)
                {
                    auto image = ftk::Image::create(imageSize, ftk::ImageType::RGBA_U8);
                    uint8_t* p = image->getData();
                    for (size_t i = 0; i < image->getByteCount(); i += 4)
                    {
                        p[i] = p[i + 1] = p[i + 2] = value;
                        p[i + 3] = 255;
                    }
                    VideoLayer layer;
                    layer.image = image;
                    VideoFrame out;
                    out.size = imageSize;
                    out.layers.push_back(layer);
                    return out;
                };
            // A pixel counted from the top left, whichever way up the
            // image's rows are: bottom up, as OpenGL reads them back, unless
            // the layout's mirror says they are top down.
            const auto pixel = [](const std::shared_ptr<ftk::Image>& image, int x, int y)
                {
                    const ftk::ImageInfo& info = image->getInfo();
                    const int row = info.layout.mirror.y ? y : info.size.h - 1 - y;
                    const uint8_t* p = image->getData() +
                        (row * info.size.w + x) * 4;
                    return std::array<int, 4>({ p[0], p[1], p[2], p[3] });
                };
            const auto near = [](const std::array<int, 4>& a, const std::array<int, 4>& b)
                {
                    for (size_t i = 0; i < 4; ++i)
                    {
                        if (std::abs(a[i] - b[i]) > 1)
                            return false;
                    }
                    return true;
                };

            auto render = gl::OffscreenRender::create(_context);
            const VideoFrame a = opaqueFrame(200);
            const VideoFrame b = opaqueFrame(100);

            // One frame, at its own size.
            auto image = render->render({ a });
            FTK_CHECK(image);
            FTK_CHECK(imageSize == image->getSize());
            FTK_CHECK(ftk::ImageType::RGBA_U8 == image->getType());
            FTK_CHECK(near({ 200, 200, 200, 255 }, pixel(image, 10, 10)));

            // Two side by side: twice as wide, A on the left and B on the
            // right.
            CompareOptions compareOptions;
            compareOptions.compare = Compare::Horizontal;
            image = render->render({ a, b }, compareOptions);
            FTK_CHECK(ftk::Size2I(imageSize.w * 2, imageSize.h) == image->getSize());
            FTK_CHECK(near({ 200, 200, 200, 255 }, pixel(image, 10, 10)));
            FTK_CHECK(near({ 100, 100, 100, 255 }, pixel(image, imageSize.w + 10, 10)));

            // One above the other: A on top. Which way up the picture comes
            // back is checked here, where it shows.
            CompareOptions verticalOptions;
            verticalOptions.compare = Compare::Vertical;
            image = render->render({ a, b }, verticalOptions);
            FTK_CHECK(ftk::Size2I(imageSize.w, imageSize.h * 2) == image->getSize());
            FTK_CHECK(near({ 200, 200, 200, 255 }, pixel(image, 10, 5)));
            FTK_CHECK(near({ 100, 100, 100, 255 }, pixel(image, 10, imageSize.h + 5)));

            // A wipe, which draws each side through a stencil mask: A on the
            // left of the line and B on the right, not B over all of it.
            CompareOptions wipeOptions;
            wipeOptions.compare = Compare::Wipe;
            image = render->render({ a, b }, wipeOptions);
            FTK_CHECK(near({ 200, 200, 200, 255 }, pixel(image, 4, imageSize.h / 2)));
            FTK_CHECK(near({ 100, 100, 100, 255 }, pixel(image, imageSize.w - 4, imageSize.h / 2)));

            // Fitted into a square: the same picture, half the size, with
            // nothing drawn above and below it.
            image = render->render({ a, b }, compareOptions, ftk::Size2I(64, 64));
            _print(ftk::Format("Offscreen render: {0}, left side {1} {2} {3} {4}").
                arg(image->getSize()).
                arg(pixel(image, 10, 32)[0]).
                arg(pixel(image, 10, 32)[1]).
                arg(pixel(image, 10, 32)[2]).
                arg(pixel(image, 10, 32)[3]));
            FTK_CHECK(ftk::Size2I(64, 64) == image->getSize());
            FTK_CHECK(near({ 0, 0, 0, 0 }, pixel(image, 10, 2)));
            FTK_CHECK(near({ 200, 200, 200, 255 }, pixel(image, 10, 32)));
            FTK_CHECK(near({ 100, 100, 100, 255 }, pixel(image, 54, 32)));
            FTK_CHECK(near({ 0, 0, 0, 0 }, pixel(image, 10, 61)));

            // The buffers of the last two output sizes are kept: with a
            // renderer of its own, a second size adds a buffer, the first
            // size back again adds none, and a third replaces one. (On a
            // software renderer each output buffer has a buffer of its own
            // for reading back, so the second size may add two.)
            {
                auto render2 = gl::OffscreenRender::create(_context);
                render2->render({ a }, CompareOptions(), ftk::Size2I(64, 64));
                const size_t count = ftk::gl::OffscreenBuffer::getObjectCount();
                render2->render({ a }, CompareOptions(), ftk::Size2I(32, 32));
                const size_t count2 = ftk::gl::OffscreenBuffer::getObjectCount();
                FTK_CHECK(count2 > count);
                render2->render({ a }, CompareOptions(), ftk::Size2I(64, 64));
                FTK_CHECK(count2 == ftk::gl::OffscreenBuffer::getObjectCount());
                render2->render({ a }, CompareOptions(), ftk::Size2I(32, 32));
                FTK_CHECK(count2 == ftk::gl::OffscreenBuffer::getObjectCount());
                render2->render({ a }, CompareOptions(), ftk::Size2I(16, 16));
                FTK_CHECK(count2 == ftk::gl::OffscreenBuffer::getObjectCount());
            }

            // Frames rendered again are kept as textures: rendered once
            // more, with other options, they are not copied in again. A
            // new frame is kept from its second render in a row, so that
            // a sequence of frames each rendered once keeps none.
            FTK_CHECK(render->getCacheByteCount() > 0);
            const size_t cacheCount = ftk::gl::Render::getTextureCacheCount();
            FTK_CHECK(cacheCount >= 2);
            render->render({ a, b }, verticalOptions, ftk::Size2I(32, 32));
            FTK_CHECK(cacheCount == ftk::gl::Render::getTextureCacheCount());
            const VideoFrame c = opaqueFrame(50);
            image = render->render({ c });
            FTK_CHECK(cacheCount == ftk::gl::Render::getTextureCacheCount());
            FTK_CHECK(near({ 50, 50, 50, 255 }, pixel(image, 10, 10)));
            image = render->render({ c });
            FTK_CHECK(cacheCount + 1 == ftk::gl::Render::getTextureCacheCount());
            FTK_CHECK(near({ 50, 50, 50, 255 }, pixel(image, 10, 10)));

            // Frames are told apart by the image, not its pixels: changed in
            // place, it renders as it was until the cache is cleared, and
            // with no cache it renders as it is.
            const auto fill = [](const VideoFrame& frame, uint8_t value)
                {
                    const auto& image = frame.layers.front().image;
                    uint8_t* p = image->getData();
                    for (size_t i = 0; i < image->getByteCount(); i += 4)
                    {
                        p[i] = p[i + 1] = p[i + 2] = value;
                    }
                };
            fill(c, 150);
            image = render->render({ c });
            FTK_CHECK(near({ 50, 50, 50, 255 }, pixel(image, 10, 10)));
            render->clearCache();
            image = render->render({ c });
            FTK_CHECK(near({ 150, 150, 150, 255 }, pixel(image, 10, 10)));
            render->setCacheByteCount(0);
            FTK_CHECK(0 == render->getCacheByteCount());
            fill(c, 250);
            image = render->render({ c });
            FTK_CHECK(near({ 250, 250, 250, 255 }, pixel(image, 10, 10)));
            render->setCacheByteCount(ftk::megabyte * 512);

            // Read back as floating point, for values past one, where
            // OpenGL can: OpenGL ES reads back bytes only, and says so.
            if (ftk::gl::OffscreenBuffer::canRead(ftk::ImageType::RGBA_F32))
            {
                image = render->render({ a }, CompareOptions(), ftk::Size2I(), {}, {},
                    ftk::ImageType::RGBA_F32);
                FTK_CHECK(ftk::ImageType::RGBA_F32 == image->getType());
            }
            else
            {
                try
                {
                    render->render({ a }, CompareOptions(), ftk::Size2I(), {}, {},
                        ftk::ImageType::RGBA_F32);
                    FTK_CHECK(false);
                }
                catch (const std::exception&)
                {}
            }

            // What cannot be done says so.
            try
            {
                render->render({});
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
            try
            {
                render->render({ a }, CompareOptions(), ftk::Size2I(), {}, {},
                    ftk::ImageType::YUV_420P_U8);
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
        }

        //! A layer holding two images, which is a clip dissolving into the
        //! one after it.
        void RenderTest::_dissolve()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());

            VideoLayer layer;
            layer.image = createImage(0);
            layer.imageB = createImage(255);
            layer.transition = Transition::Dissolve;
            VideoFrame frame;
            frame.size = imageSize;
            frame.layers.push_back(layer);

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            const std::vector<ftk::Box2I> boxes =
            {
                ftk::Box2I(0, 0, imageSize.w, imageSize.h)
            };
            // Part way through, and at either end.
            for (float value : { 0.F, .5F, 1.F })
            {
                frame.layers[0].transitionValue = value;
                render->begin(imageSize);
                render->drawVideo({ frame }, boxes);
                render->end();
                _print(ftk::Format("Dissolve: {0}").arg(value));
            }
        }

        //! The color operations, which are a shader each and are otherwise
        //! only reached by turning them on in the viewport.
        void RenderTest::_display()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());

            const std::vector<VideoFrame> frames = { createFrame(128) };
            const std::vector<ftk::Box2I> boxes =
            {
                ftk::Box2I(0, 0, imageSize.w, imageSize.h)
            };

            std::vector<DisplayOptions> options;
            {
                DisplayOptions o;
                o.color.enabled = true;
                o.color.add = ftk::V3F(.1F, .1F, .1F);
                o.color.contrast = ftk::V3F(1.2F, 1.2F, 1.2F);
                options.push_back(o);
            }
            {
                DisplayOptions o;
                o.levels.enabled = true;
                o.levels.inLow = .1F;
                o.levels.inHigh = .9F;
                options.push_back(o);
            }
            {
                DisplayOptions o;
                o.exposure.enabled = true;
                o.exposure.exposure = 1.F;
                options.push_back(o);
            }
            {
                DisplayOptions o;
                o.softClip.enabled = true;
                o.softClip.value = .5F;
                options.push_back(o);
            }
            {
                DisplayOptions o;
                o.negative = true;
                o.channels = ftk::ChannelDisplay::Red;
                o.mirror.x = true;
                o.mirror.y = true;
                options.push_back(o);
            }

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            for (const auto& o : options)
            {
                render->begin(imageSize);
                render->drawVideo(frames, boxes, {}, { o });
                render->end();
            }

            // Exposure is a plain stop adjustment now: zero must leave the
            // pixels exactly alone, and one stop must double them. The old
            // exrdisplay formula failed the first of these, which is why it
            // went.
            const auto center = [&]
            {
                float rgba[4] = { 0.F, 0.F, 0.F, 0.F };
                glReadPixels(
                    imageSize.w / 2,
                    imageSize.h / 2,
                    1,
                    1,
                    GL_RGBA,
                    GL_FLOAT,
                    rgba);
                return rgba[0];
            };
            const auto renderWith = [&](const DisplayOptions& o)
            {
                render->begin(imageSize);
                render->drawVideo(frames, boxes, {}, { o });
                render->end();
                return center();
            };
            const float off = renderWith(DisplayOptions());
            DisplayOptions o;
            o.exposure.enabled = true;
            o.exposure.exposure = 0.F;
            const float zero = renderWith(o);
            o.exposure.exposure = 1.F;
            const float one = renderWith(o);
            _print(ftk::Format("Exposure off: {0}, zero: {1}, one stop: {2}").
                arg(off).
                arg(zero).
                arg(one));
            FTK_CHECK(zero == off);
            FTK_CHECK(std::abs(one - off * 2.F) < .01F);
        }

        //! The drawing the renderer passes through to the one underneath it.
        void RenderTest::_oneToOne()
        {
            // A picture drawn at its own size must come back exactly as it
            // went in, whatever the filters: this is what an export at the
            // source size does, and any resampling at all shows as soft or
            // shifted edges against the source in a wipe (DJV #876). Hard
            // black and white edges, at even and odd columns, in an odd
            // sized image -- where a half pixel error in placing the box
            // has nowhere to hide.
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            const ftk::Size2I size(63, 17);
            auto image = ftk::Image::create(size, ftk::ImageType::RGBA_U8);
            for (int y = 0; y < size.h; ++y)
            {
                uint8_t* p = image->getData() + y * size.w * 4;
                for (int x = 0; x < size.w; ++x)
                {
                    // Stripes one, two and three wide, then a block.
                    const bool white =
                        (x >= 3 && x < 4) ||
                        (x >= 7 && x < 9) ||
                        (x >= 12 && x < 15) ||
                        (x >= 20 && x < 41);
                    const uint8_t v = white ? 255 : 0;
                    p[x * 4 + 0] = v;
                    p[x * 4 + 1] = v;
                    p[x * 4 + 2] = v;
                    p[x * 4 + 3] = 255;
                }
            }
            VideoLayer layer;
            layer.image = image;
            VideoFrame frame;
            frame.size = size;
            frame.layers.push_back(layer);
            const std::vector<ftk::Box2I> boxes = { ftk::Box2I(0, 0, size.w, size.h) };

            auto buffer = ftk::gl::OffscreenBuffer::create(
                size,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            for (auto minify : ftk::getImageFilterEnums())
            {
                for (auto magnify : ftk::getImageFilterEnums())
                {
                    ftk::ImageOptions imageOptions;
                    imageOptions.imageFilters.minify = minify;
                    imageOptions.imageFilters.magnify = magnify;
                    render->begin(size);
                    render->drawVideo({ frame }, boxes, { imageOptions });
                    render->end();

                    std::vector<uint8_t> pixels(size.w * size.h * 4);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(0, 0, size.w, size.h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    // Read back bottom up; the source is top down. Compare
                    // the middle row either way, which is the same.
                    const int row = size.h / 2;
                    int differ = 0;
                    std::string got;
                    for (int x = 0; x < size.w; ++x)
                    {
                        const uint8_t a = image->getData()[(row * size.w + x) * 4];
                        const uint8_t b = pixels[(row * size.w + x) * 4];
                        if (a != b)
                        {
                            ++differ;
                        }
                        if (x < 16)
                        {
                            got += ftk::Format("{0} ").arg(static_cast<int>(b)).str();
                        }
                    }
                    _print(ftk::Format("One to one, minify {0}, magnify {1}: {2} "
                        "columns differ; first columns {3}").
                        arg(minify).
                        arg(magnify).
                        arg(differ).
                        arg(got));
                    FTK_CHECK(0 == differ);
                }
            }
        }

        void RenderTest::_yuvLevels()
        {
            // Flat YUV pictures in every planar type, at the black, grey,
            // and white of each range, must come back as the RGB they stand
            // for. Legal range over eight bits keeps the 8-bit levels in its
            // high byte, and taking it for 8-bit levels read 10-bit white
            // as 253.
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            const ftk::Size2I size(8, 4);
            auto buffer = ftk::gl::OffscreenBuffer::create(
                size,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);

            struct Level
            {
                ftk::VideoLevels levels;
                int y;
                int c;
                uint8_t rgb;
            };
            // 8-bit code values; the 16-bit types take them times 256, the
            // way a 10-bit file shifted up to sixteen bits has them.
            const std::vector<Level> levels =
            {
                { ftk::VideoLevels::LegalRange, 16, 128, 0 },
                { ftk::VideoLevels::LegalRange, 126, 128, 128 },
                { ftk::VideoLevels::LegalRange, 235, 128, 255 },
                { ftk::VideoLevels::FullRange, 0, 128, 0 },
                { ftk::VideoLevels::FullRange, 255, 128, 255 }
            };
            std::vector<ftk::ImageType> types =
            {
                ftk::ImageType::YUV_420P_U8,
                ftk::ImageType::YUV_422P_U8,
                ftk::ImageType::YUV_444P_U8,
                ftk::ImageType::YUV_420SP_U8
            };
            if (!ftk::gl::isGLES())
            {
                // ES 3.0 has no 16-bit normalized textures, so feather-tk
                // does not support the U16 types there.
                types.push_back(ftk::ImageType::YUV_420P_U16);
                types.push_back(ftk::ImageType::YUV_422P_U16);
                types.push_back(ftk::ImageType::YUV_444P_U16);
                types.push_back(ftk::ImageType::YUV_420SP_U16);
            }
            for (const auto type : types)
            {
                const bool u16 =
                    ftk::ImageType::YUV_420P_U16 == type ||
                    ftk::ImageType::YUV_422P_U16 == type ||
                    ftk::ImageType::YUV_444P_U16 == type ||
                    ftk::ImageType::YUV_420SP_U16 == type;
                // The 16-bit types also as 12-bit values in 16-bit words,
                // the way a 12-bit file is read: the same code values,
                // over 4095 rather than 65535, with the depth set.
                for (const int depth : u16 ? std::vector<int>{ 0, 12 } : std::vector<int>{ 0 })
                for (const auto& level : levels)
                {
                    ftk::ImageInfo info(size, type);
                    info.videoLevels = level.levels;
                    info.depth = depth;
                    auto image = ftk::Image::create(info);
                    // The luma plane, then the chroma, whichever way the
                    // chroma is laid out: it is all the same value.
                    const size_t lumaCount = size.w * size.h;
                    if (u16)
                    {
                        const size_t count = image->getByteCount() / 2;
                        uint16_t* p = reinterpret_cast<uint16_t*>(image->getData());
                        // Full range white is the top of the 16-bit range.
                        const int y = 255 == level.y && ftk::VideoLevels::FullRange == level.levels ?
                            65535 :
                            level.y * 256;
                        const int shift = depth ? 16 - depth : 0;
                        for (size_t i = 0; i < count; ++i)
                        {
                            p[i] = (i < lumaCount ? y : level.c * 256) >> shift;
                        }
                    }
                    else
                    {
                        const size_t count = image->getByteCount();
                        uint8_t* p = image->getData();
                        for (size_t i = 0; i < count; ++i)
                        {
                            p[i] = i < lumaCount ? level.y : level.c;
                        }
                    }
                    VideoLayer layer;
                    layer.image = image;
                    VideoFrame frame;
                    frame.size = size;
                    frame.layers.push_back(layer);
                    render->begin(size);
                    render->drawVideo(
                        { frame },
                        { ftk::Box2I(0, 0, size.w, size.h) },
                        { ftk::ImageOptions() });
                    render->end();

                    std::vector<uint8_t> pixels(size.w * size.h * 4);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(0, 0, size.w, size.h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    const uint8_t* rgb = pixels.data() + ((size.h / 2) * size.w + size.w / 2) * 4;
                    _print(ftk::Format("YUV levels, {0} {1} depth {7}, Y {2}: {3} {4} {5}, expected {6}").
                        arg(type).
                        arg(level.levels).
                        arg(level.y).
                        arg(static_cast<int>(rgb[0])).
                        arg(static_cast<int>(rgb[1])).
                        arg(static_cast<int>(rgb[2])).
                        arg(static_cast<int>(level.rgb)).
                        arg(depth));
                    for (int i = 0; i < 3; ++i)
                    {
                        FTK_CHECK(level.rgb == rgb[i]);
                    }
                }
            }
        }

        void RenderTest::_prims()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            auto fontSystem = _context->getSystem<ftk::FontSystem>();

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            render->begin(imageSize);

            const ftk::Color4F color(1.F, 1.F, 1.F);
            const ftk::Box2F box(0.F, 0.F, 10.F, 10.F);
            render->drawRect(box, color);
            render->drawRects({ box }, color);
            render->drawLine(ftk::V2F(0.F, 0.F), ftk::V2F(10.F, 10.F), color);
            render->drawLines({ { ftk::V2F(0.F, 0.F), ftk::V2F(10.F, 10.F) } }, color);
            render->drawMesh(ftk::mesh(box), color);
            render->drawColorMesh(ftk::mesh(box), color);
            auto texture = ftk::gl::Texture::create(
                ftk::ImageInfo(imageSize, ftk::ImageType::RGBA_U8));
            render->drawTexture(texture->getID(), ftk::Box2I(0, 0, 10, 10));
            render->drawImage(createImage(255), box);
            render->drawImage(createImage(255), ftk::mesh(box));

            const ftk::FontInfo fontInfo;
            const auto metrics = fontSystem->getMetrics(fontInfo);
            render->drawText(
                fontSystem->getGlyphs("Test", fontInfo),
                metrics,
                ftk::V2F(0.F, 0.F),
                color);

            // The state the viewport sets around its drawing.
            render->setViewport(ftk::Box2I(0, 0, imageSize.w, imageSize.h));
            FTK_CHECK(render->getViewport().w() == imageSize.w);
            render->setClipRectEnabled(true);
            FTK_CHECK(render->getClipRectEnabled());
            render->setClipRect(ftk::Box2I(0, 0, 10, 10));
            FTK_CHECK(render->getClipRect().w() == 10);
            render->setClipRectEnabled(false);
            render->setTransform(render->getTransform());
            render->clearViewport(ftk::Color4F(0.F, 0.F, 0.F));
            render->getDiag();

            render->end();
        }

        void RenderTest::_compare()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());

            // Two frames, so that the comparisons that combine a pair have a
            // pair to combine, and one that is different from the other so
            // that the difference is not uniformly zero.
            const std::vector<VideoFrame> frames =
            {
                createFrame(0),
                createFrame(255)
            };
            const AspectRatioOptions aspectRatio;

            for (auto compare : getCompareEnums())
            {
                CompareOptions compareOptions;
                compareOptions.compare = compare;
                // Off center, so that the wipe and the butterfly are drawn
                // with both halves rather than one of them empty.
                compareOptions.wipeCenter = ftk::V2F(.4F, .6F);
                compareOptions.wipeRotation = 45.F;
                compareOptions.overlay = .5F;
                compareOptions.differenceGain = 2.F;

                const ftk::Size2I renderSize =
                    getRenderSize(compareOptions, aspectRatio, frames);
                FTK_CHECK(renderSize.isValid());
                const std::vector<ftk::Box2I> boxes =
                    getBoxes(compareOptions, aspectRatio, frames);
                FTK_CHECK(!boxes.empty());

                auto buffer = ftk::gl::OffscreenBuffer::create(
                    renderSize,
                    ftk::gl::getOffscreenColorDefault());
                ftk::gl::OffscreenBufferBinding bufferBinding(buffer);

                render->begin(renderSize);
                render->drawVideo(
                    frames,
                    boxes,
                    { ftk::ImageOptions(), ftk::ImageOptions() },
                    { DisplayOptions(), DisplayOptions() },
                    compareOptions);
                render->end();

                _print(ftk::Format("Compare {0}: {1}").
                    arg(compare).
                    arg(renderSize));
            }

            // A single frame, which is what the comparisons fall back to when
            // there is nothing to compare with.
            const std::vector<VideoFrame> one = { createFrame(128) };
            for (auto compare : getCompareEnums())
            {
                CompareOptions compareOptions;
                compareOptions.compare = compare;
                const ftk::Size2I renderSize =
                    getRenderSize(compareOptions, aspectRatio, one);
                // Some comparisons have no size of their own with nothing to
                // compare against.
                if (!renderSize.isValid())
                {
                    _print(ftk::Format("Compare {0}: no size with one frame").
                        arg(compare));
                    continue;
                }
                auto buffer = ftk::gl::OffscreenBuffer::create(
                    renderSize,
                    ftk::gl::getOffscreenColorDefault());
                ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
                render->begin(renderSize);
                render->drawVideo(
                    one,
                    getBoxes(compareOptions, aspectRatio, one),
                    {},
                    {},
                    compareOptions);
                render->end();
            }

            // Nothing to draw at all.
            {
                auto buffer = ftk::gl::OffscreenBuffer::create(
                    imageSize,
                    ftk::gl::getOffscreenColorDefault());
                ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
                render->begin(imageSize);
                render->drawVideo({}, {});
                render->end();
            }
        }

        void RenderTest::_background()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            const std::vector<ftk::Box2I> boxes =
            {
                ftk::Box2I(0, 0, imageSize.w, imageSize.h)
            };
            for (auto background : getBackgroundEnums())
            {
                BackgroundOptions options;
                options.type = background;
                render->begin(imageSize);
                render->drawBackground(
                    boxes,
                    ftk::M44F(),
                    options,
                    CompareOptions());
                render->end();
                _print(ftk::Format("Background: {0}").arg(background));
            }
        }

        void RenderTest::_foreground()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            const std::vector<ftk::Box2I> boxes =
            {
                ftk::Box2I(0, 0, imageSize.w, imageSize.h)
            };

            for (auto cellMode : getGridCellModeEnums())
            {
                for (auto labels : getGridLabelsEnums())
                {
                    ForegroundOptions options;
                    options.grid.enabled = true;
                    options.grid.cellMode = cellMode;
                    options.grid.labels = labels;
                    options.centerMarker.enabled = true;
                    options.missingIndicator.enabled = true;
                    render->begin(imageSize);
                    render->drawForeground(
                        boxes,
                        ftk::M44F(),
                        options,
                        CompareOptions());
                    render->end();
                }
                _print(ftk::Format("Grid: {0}").arg(cellMode));
            }

            // And with everything turned off, which is the usual case.
            render->begin(imageSize);
            render->drawForeground(
                boxes,
                ftk::M44F(),
                ForegroundOptions(),
                CompareOptions());
            render->end();

            // The clipping warning reads the rendered video from a texture;
            // a second buffer stands in for it, since reading the bound one
            // would be a feedback loop.
            {
                auto video = ftk::gl::OffscreenBuffer::create(
                    imageSize,
                    ftk::gl::getOffscreenColorDefault());
                ClippingWarning options;
                options.enabled = true;
                options.low = .1F;
                options.high = .9F;
                render->begin(imageSize);
                render->drawClippingWarning(
                    video->getColorID(),
                    ftk::Box2I(ftk::V2I(), imageSize),
                    true,
                    boxes,
                    ftk::M44F(),
                    options);
                render->end();
            }
        }

        //! The color configuration and the look-up table, which are the two
        //! things the renderer builds a shader from at run time rather than
        //! having one ready.
        void RenderTest::_color()
        {
#if defined(TLRENDER_OCIO)
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            const std::vector<VideoFrame> frames = { createFrame(128) };
            const std::vector<ftk::Box2I> boxes =
            {
                ftk::Box2I(0, 0, imageSize.w, imageSize.h)
            };
            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::getOffscreenColorDefault());
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);

            try
            {
                // Whatever the built-in configuration calls its display and
                // view, rather than names that may not be in every version
                // of it.
                auto config = OCIO::Config::CreateFromFile("ocio://default");
                FTK_CHECK(config);
                const std::string display = config->getDefaultDisplay();
                const std::string view = config->getDefaultView(display.c_str());
                const std::string input = config->getColorSpaceNameByIndex(0);
                _print(ftk::Format("OCIO: {0}, {1}, {2}").
                    arg(input).arg(display).arg(view));

                OCIOOptions options;
                options.enabled = true;
                options.config = OCIOConfig::BuiltIn;
                options.input = input;
                options.display = display;
                options.view = view;
                render->setOCIOOptions(options);
                render->begin(imageSize);
                render->drawVideo(frames, boxes);
                render->end();

                // And off again, which throws the shader away.
                render->setOCIOOptions(OCIOOptions());
                render->begin(imageSize);
                render->drawVideo(frames, boxes);
                render->end();
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            try
            {
                for (auto order : getLUTOrderEnums())
                {
                    LUTOptions options;
                    options.enabled = true;
                    options.fileName =
                        std::string(TLRENDER_SAMPLE_DATA) + "/LUT_SRGB_256.lut";
                    options.order = order;
                    render->setLUTOptions(options);
                    render->begin(imageSize);
                    render->drawVideo(frames, boxes);
                    render->end();
                    _print(ftk::Format("LUT: {0}").arg(order));
                }
                render->setLUTOptions(LUTOptions());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // A LUT or an OCIO configuration that cannot be read leaves the
            // picture as it is. Both used to throw from the options, which
            // the viewport met in the middle of its draw and so showed
            // nothing at all.
            try
            {
                const std::string missing = ftk::fromFileSystem(
                    std::filesystem::temp_directory_path() / "tlRenderTestMissing");
                // What the frame draws as with neither, which the frame
                // drawn with a missing one has to match.
                const auto draw = [&render, &frames, &boxes]
                {
                    render->begin(imageSize);
                    render->drawVideo(frames, boxes);
                    render->end();
                    std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
                    glReadPixels(
                        imageSize.w / 2, imageSize.h / 2, 1, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                    return pixel;
                };
                const std::array<uint8_t, 4> unaltered = draw();

                LUTOptions lutOptions;
                lutOptions.enabled = true;
                lutOptions.fileName = missing + ".cube";
                render->setLUTOptions(lutOptions);
                FTK_CHECK(draw() == unaltered);
                render->setLUTOptions(LUTOptions());

                OCIOOptions ocioOptions;
                ocioOptions.enabled = true;
                ocioOptions.config = OCIOConfig::File;
                ocioOptions.fileName = missing + ".ocio";
                ocioOptions.input = "scene_linear";
                ocioOptions.display = "sRGB";
                ocioOptions.view = "Raw";
                render->setOCIOOptions(ocioOptions);
                FTK_CHECK(draw() == unaltered);
                render->setOCIOOptions(OCIOOptions());
            }
            catch (const std::exception& e)
            {
                // Throwing is the failure this is here to find, and _error()
                // alone does not fail the test.
                _error(e.what());
                const bool threw = true;
                FTK_CHECK(!threw);
            }
#endif // TLRENDER_OCIO
        }

        //! The renderer against the CPU: the same pixels drawn through the
        //! display shader and through CPUColor, for the two to match --
        //! what keeps the thumbnails, and any renderer, honest.
        void RenderTest::_cpuColor()
        {
#if defined(TLRENDER_OCIO)
            if (!ftk::gl::OffscreenBuffer::canRead(ftk::ImageType::RGBA_F32))
            {
                _print("CPU color: no floating point read back, skipped");
                return;
            }
            auto render = gl::OffscreenRender::create(_context);

            // Ramps of every channel, a little past one at the end.
            const ftk::Size2I size(32, 16);
            auto image = ftk::Image::create(size, ftk::ImageType::RGBA_F32);
            float* pixels = reinterpret_cast<float*>(image->getData());
            for (int y = 0; y < size.h; ++y)
            {
                for (int x = 0; x < size.w; ++x)
                {
                    float* p = pixels + (y * size.w + x) * 4;
                    p[0] = x / static_cast<float>(size.w - 1) * 1.1F;
                    p[1] = y / static_cast<float>(size.h - 1);
                    p[2] = (x + y) / static_cast<float>(size.w + size.h - 2);
                    p[3] = 1.F;
                }
            }
            VideoLayer layer;
            layer.image = image;
            VideoFrame frame;
            frame.size = size;
            frame.layers.push_back(layer);

            auto config = OCIO::Config::CreateFromFile("ocio://default");
            OCIOOptions ocio;
            ocio.enabled = true;
            ocio.config = OCIOConfig::BuiltIn;
            ocio.input = "ACEScg";
            ocio.display = config->getDefaultDisplay();
            ocio.view = config->getDefaultView(ocio.display.c_str());
            LUTOptions lut;
            lut.enabled = true;
            lut.fileName = std::string(TLRENDER_SAMPLE_DATA) + "/LUT_SRGB_256.lut";
            DisplayOptions corrections;
            corrections.color.enabled = true;
            corrections.color.add = ftk::V3F(.02F, 0.F, -.01F);
            corrections.color.contrast = ftk::V3F(1.2F, 1.2F, 1.2F);
            corrections.color.saturation = ftk::V3F(1.3F, 1.3F, 1.3F);
            corrections.color.hue = 10.F;
            corrections.levels.enabled = true;
            corrections.levels.inLow = .05F;
            corrections.levels.inHigh = .9F;
            corrections.levels.gamma = 1.3F;
            corrections.exposure.enabled = true;
            corrections.exposure.exposure = .5F;
            corrections.softClip.enabled = true;
            corrections.softClip.value = .2F;
            DisplayOptions channels;
            channels.negative = true;
            channels.channels = ftk::ChannelDisplay::Red;

            struct Case
            {
                std::string name;
                OCIOOptions ocio;
                LUTOptions lut;
                DisplayOptions display;
            };
            std::vector<Case> cases;
            cases.push_back({ "nothing", OCIOOptions(), LUTOptions(), DisplayOptions() });
            cases.push_back({ "corrections", OCIOOptions(), LUTOptions(), corrections });
            cases.push_back({ "negative and channels", OCIOOptions(), LUTOptions(), channels });
            cases.push_back({ "LUT", OCIOOptions(), lut, DisplayOptions() });
            lut.order = LUTOrder::PreConfig;
            cases.push_back({ "OCIO", ocio, LUTOptions(), DisplayOptions() });
            cases.push_back({ "OCIO, LUT before, corrections", ocio, lut, corrections });
            lut.order = LUTOrder::PostConfig;
            cases.push_back({ "OCIO, LUT after, corrections", ocio, lut, corrections });

            for (const auto& c : cases)
            {
                try
                {
                    render->setOCIOOptions(c.ocio);
                    render->setLUTOptions(c.lut);
                    const auto out = render->render(
                        { frame },
                        CompareOptions(),
                        size,
                        { ftk::ImageOptions() },
                        { c.display },
                        ftk::ImageType::RGBA_F32);

                    std::vector<float> expected(pixels, pixels + size.w * size.h * 4);
                    CPUColor(c.ocio, c.lut, c.display).apply(expected.data(), size.w * size.h);

                    // The rows come back in the order they went in, unless
                    // the layout says they were turned over. The renderer
                    // may draw through a half float buffer, and OCIO's
                    // shaders are not its CPU arithmetic to the last bit, so
                    // the difference allowed grows a little with the value;
                    // a stage left out or out of order is far past it.
                    const ftk::ImageInfo& info = out->getInfo();
                    const float* result = reinterpret_cast<const float*>(out->getData());
                    float maxDiff = 0.F;
                    bool ok = true;
                    for (int y = 0; y < size.h; ++y)
                    {
                        const int row = info.layout.mirror.y ? size.h - 1 - y : y;
                        for (int x = 0; x < size.w; ++x)
                        {
                            for (int i = 0; i < 3; ++i)
                            {
                                const float a = result[(row * size.w + x) * 4 + i];
                                const float b = expected[(y * size.w + x) * 4 + i];
                                const float d = std::abs(a - b);
                                maxDiff = std::max(maxDiff, d);
                                if (d > .002F + .002F * std::abs(b))
                                {
                                    ok = false;
                                }
                            }
                        }
                    }
                    _print(ftk::Format("CPU color, {0}: largest difference {1}").
                        arg(c.name).arg(maxDiff, 5));
                    FTK_CHECK(ok);
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                    const bool threw = true;
                    FTK_CHECK(!threw);
                }
            }
            render->setOCIOOptions(OCIOOptions());
            render->setLUTOptions(LUTOptions());
#endif // TLRENDER_OCIO
        }
    }
}
