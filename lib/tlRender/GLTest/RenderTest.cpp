// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GLTest/RenderTest.h>

#include <tlRender/GL/Render.h>

#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Transition.h>

#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Texture.h>
#include <ftk/GL/Window.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>
#include <ftk/Core/Mesh.h>
#include <ftk/Core/Format.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
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
            _dissolve();
            _display();
            _background();
            _foreground();
            _prims();
            _color();
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
                ftk::gl::offscreenColorDefault);
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
                // The knee, which nothing else reaches.
                DisplayOptions o;
                o.exposure.enabled = true;
                o.exposure.exposure = 1.F;
                o.exposure.defog = .1F;
                o.exposure.kneeLow = .5F;
                o.exposure.kneeHigh = 2.F;
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
                ftk::gl::offscreenColorDefault);
            ftk::gl::OffscreenBufferBinding bufferBinding(buffer);
            for (const auto& o : options)
            {
                render->begin(imageSize);
                render->drawVideo(frames, boxes, {}, { o });
                render->end();
            }
        }

        //! The drawing the renderer passes through to the one underneath it.
        void RenderTest::_prims()
        {
            auto window = createWindow(_context);
            auto render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            auto fontSystem = _context->getSystem<ftk::FontSystem>();

            auto buffer = ftk::gl::OffscreenBuffer::create(
                imageSize,
                ftk::gl::offscreenColorDefault);
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
                    ftk::gl::offscreenColorDefault);
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
                    ftk::gl::offscreenColorDefault);
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
                    ftk::gl::offscreenColorDefault);
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
                ftk::gl::offscreenColorDefault);
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
                ftk::gl::offscreenColorDefault);
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
                ftk::gl::offscreenColorDefault);
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
#endif // TLRENDER_OCIO
        }
    }
}
