// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "App.h"

#include "TestPatterns.h"

#include <tlRender/GL/Render.h>

#include <tlRender/IO/System.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Util.h>
#include <ftk/GL/Window.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/track.h>
#include <opentimelineio/timeline.h>

namespace tl
{
    namespace examples
    {
        namespace test_patterns
        {
            void App::_init(
                const std::shared_ptr<ftk::Context>& context,
                std::vector<std::string>& argv)
            {
                IApp::_init(
                    context,
                    argv,
                    "test-patterns",
                    "Example test patterns application.");
            }

            App::App()
            {}

            App::~App()
            {}

            std::shared_ptr<App> App::create(
                const std::shared_ptr<ftk::Context>& context,
                std::vector<std::string>& argv)
            {
                auto out = std::shared_ptr<App>(new App);
                out->_init(context, argv);
                return out;
            }

            void App::run()
            {
                _window = ftk::gl::Window::create(
                    _context,
                    "test-patterns",
                    ftk::Size2I(1, 1),
                    static_cast<int>(ftk::gl::WindowOptions::MakeCurrent));

                for (const auto& size : {
                    ftk::Size2I(1920, 1080),
                    ftk::Size2I(3840, 2160),
                    ftk::Size2I(4096, 2160)
                    })
                {
                    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(new OTIO_NS::Timeline);
                    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> otioTrack(new OTIO_NS::Track);
                    otioTimeline->tracks()->append_child(otioTrack);

                    for (const auto& name : {
                        CountTestPattern::getClassName(),
                        SwatchesTestPattern::getClassName(),
                        GridTestPattern::getClassName()
                        })
                    {
                        //const std::string output = ftk::Format("{0}_{1}_{2}_pattern.mp4").arg(name).arg(size.w).arg(size.h);
                        const std::string output = ftk::Format("{0}_{1}_{2}.0.dpx").arg(name).arg(size.w).arg(size.h);
                        std::cout << "Output: " << output << std::endl;
                        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> otioClip(new OTIO_NS::Clip);
                        //OTIO_NS::SerializableObject::Retainer<OTIO_NS::MediaReference> mediaReference(
                        //    new OTIO_NS::ExternalReference(ftk::Format("{0}").arg(output)));
                        OTIO_NS::SerializableObject::Retainer<OTIO_NS::ImageSequenceReference> mediaReference(
                            new OTIO_NS::ImageSequenceReference(
                                "file://",
                                ftk::Path(output).getBase(),
                                ftk::Path(output).getExt(),
                                0,
                                1,
                                24));
                        const OTIO_NS::TimeRange timeRange(
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(24 * 3, 24.0));
                        mediaReference->set_available_range(timeRange);
                        otioClip->set_media_reference(mediaReference);
                        otioTrack->append_child(otioClip);

                        // Create the I/O plugin.
                        auto writerPlugin = _context->getSystem<WriteSystem>()->getPlugin(ftk::Path(output));
                        if (!writerPlugin)
                        {
                            throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
                        }
                        ftk::ImageInfo info;
                        info.size.w = size.w;
                        info.size.h = size.h;
                        info.type = ftk::ImageType::RGB_U10;
                        info = writerPlugin->getInfo(info);
                        if (ftk::ImageType::None == info.type)
                        {
                            throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
                        }
                        IOInfo ioInfo;
                        ioInfo.video.push_back(info);
                        ioInfo.videoTime = timeRange;
                        auto writer = writerPlugin->write(ftk::Path(output), ioInfo);
                        if (!writer)
                        {
                            throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
                        }

                        // Create the offscreen buffer.
                        auto buffer = ftk::gl::OffscreenBuffer::create(size, ftk::gl::TextureType::RGBA_F32);
                        ftk::gl::OffscreenBufferBinding binding(buffer);
                        auto image = ftk::Image::create(info);

                        // Render the test pattern.
                        auto render = gl::Render::create(
                            _context->getLogSystem(),
                            _context->getSystem<ftk::FontSystem>());
                        auto pattern = TestPatternFactory::create(_context, name, size);
                        for (double i = ioInfo.videoTime->start_time().value(); i < ioInfo.videoTime->duration().value(); i = i + 1.0)
                        {
                            const OTIO_NS::RationalTime time(i, 24.0);

                            render->begin(size);
                            pattern->render(render, time);
                            render->end();

                            // Write the image.
                            glPixelStorei(GL_PACK_ALIGNMENT, info.layout.alignment);
#if defined(FTK_API_GL_4_1)
                            glPixelStorei(GL_PACK_SWAP_BYTES, info.layout.endian != ftk::getEndian());
#endif // FTK_API_GL_4_1
                            const GLenum format = ftk::gl::getReadPixelsFormat(info.type);
                            const GLenum type = ftk::gl::getReadPixelsType(info.type);
                            if (GL_NONE == format || GL_NONE == type)
                            {
                                throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
                            }
                            glReadPixels(
                                0,
                                0,
                                info.size.w,
                                info.size.h,
                                format,
                                type,
                                image->getData());
                            writer->writeVideo(time, image);
                        }
                    }

                    otioTimeline->to_json_file(ftk::Format("{0}.otio").arg(size));
                }
            }
        }
    }
}
