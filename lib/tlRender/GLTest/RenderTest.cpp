// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GLTest/RenderTest.h>

#include <tlRender/GL/Render.h>

#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>

#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Window.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>
#include <ftk/Core/Format.h>

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
            _background();
            _foreground();
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

            ForegroundOptions options;
            options.grid.enabled = true;
            options.centerMarker.enabled = true;
            render->begin(imageSize);
            render->drawForeground(
                boxes,
                ftk::M44F(),
                options,
                CompareOptions());
            render->end();

            // And with everything turned off, which is the usual case.
            render->begin(imageSize);
            render->drawForeground(
                boxes,
                ftk::M44F(),
                ForegroundOptions(),
                CompareOptions());
            render->end();
        }
    }
}
