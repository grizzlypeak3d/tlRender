// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/BakeApp/App.h>

#include <tlRender/GL/Render.h>

#include <tlRender/IO/System.h>

#include <tlRender/Core/Time.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/Util.h>
#include <ftk/GL/Window.h>
#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Math.h>
#include <ftk/Core/String.h>

#include <algorithm>
#include <cstring>

namespace tl
{
    namespace bake
    {
        void App::_init(
            const std::shared_ptr<ftk::Context>& context,
            std::vector<std::string>& argv)
        {
            std::vector<std::string> ffmpegCodecs;
            std::vector<std::string> ffmpegAudioCodecs;
#if defined(TLRENDER_FFMPEG_PLUGIN)
            auto ioSystem = context->getSystem<WriteSystem>();
            auto ffmpegPlugin = ioSystem->getPlugin<ffmpeg::WritePlugin>();
            ffmpegCodecs = ffmpegPlugin->getCodecs();
            ffmpegAudioCodecs = ffmpegPlugin->getAudioCodecs();
#endif // TLRENDER_FFMPEG_PLUGIN
            _cmdLine.input = ftk::CmdLineArg<std::string>::create(
                "input",
                "The input timeline.");
            _cmdLine.output = ftk::CmdLineArg<std::string>::create(
                "output",
                "The output file.");
            _cmdLine.inOutRange = ftk::CmdLineOption<OTIO_NS::TimeRange>::create(
                { "-inOutRange" },
                "Set the in/out points range.",
                "Render");
            _cmdLine.renderSize = ftk::CmdLineOption<ftk::Size2I>::create(
                { "-renderSize", "-rs" },
                "Render size.",
                "Render");
            _cmdLine.outputPixelType = ftk::CmdLineOption<ftk::ImageType>::create(
                { "-outputPixelType", "-op" },
                "Output pixel type.",
                "Render",
                std::optional<ftk::ImageType>(),
                ftk::quotes(ftk::getImageTypeLabels()));
            _cmdLine.ocioFileName = ftk::CmdLineOption<std::string>::create(
                { "-ocio" },
                "OCIO configuration file name (e.g., config.ocio).",
                "Color");
            _cmdLine.ocioInput = ftk::CmdLineOption<std::string>::create(
                { "-ocioInput" },
                "OCIO input name.",
                "Color");
            _cmdLine.ocioDisplay = ftk::CmdLineOption<std::string>::create(
                { "-ocioDisplay" },
                "OCIO display namee.",
                "Color");
            _cmdLine.ocioView = ftk::CmdLineOption<std::string>::create(
                { "-ocioView" },
                "OCIO view namee.",
                "Color");
            _cmdLine.ocioLook = ftk::CmdLineOption<std::string>::create(
                { "-ocioLook" },
                "OCIO look namee.",
                "Color");
            _cmdLine.lutFileName = ftk::CmdLineOption<std::string>::create(
                { "-lut" },
                "LUT file name.",
                "Color");
            _cmdLine.lutOrder = ftk::CmdLineOption<LUTOrder>::create(
                { "-lutOrder" },
                "LUT operation order.",
                "Color",
                std::optional<LUTOrder>(),
                ftk::quotes(getLUTOrderLabels()));
            _cmdLine.sequenceDefaultSpeed = ftk::CmdLineOption<double>::create(
                { "-sequenceDefaultSpeed" },
                "Default speed for image sequences.",
                "Image Sequences",
                SeqOptions().defaultSpeed);
            _cmdLine.sequenceThreadCount = ftk::CmdLineOption<int>::create(
                { "-sequenceThreadCount" },
                "Number of threads for image sequence I/O.",
                "Image Sequences",
                static_cast<int>(Options().readThreadCount));
#if defined(TLRENDER_EXR)
            _cmdLine.exrCompression = ftk::CmdLineOption<exr::Compression>::create(
                { "-exrCompression" },
                "Output compression.",
                "OpenEXR",
                exr::Compression::ZIP,
                ftk::quotes(exr::getCompressionLabels()));
            _cmdLine.exrDWACompressionLevel = ftk::CmdLineOption<float>::create(
                { "-exrDWACompressionLevel" },
                "DWA compression level.",
                "OpenEXR",
                45.F);
#endif // TLRENDER_EXR
#if defined(TLRENDER_FFMPEG_PLUGIN)
            _cmdLine.ffmpegCodec = ftk::CmdLineOption<std::string>::create(
                { "-ffmpegCodec", "-ffc" },
                "Output codec.",
                "FFmpeg",
                "mjpeg",
                ftk::quotes(ffmpegCodecs));
            _cmdLine.ffmpegAudioCodec = ftk::CmdLineOption<std::string>::create(
                { "-ffmpegAudioCodec", "-ffac" },
                "Output audio codec. The default is chosen by the output format.",
                "FFmpeg",
                std::optional<std::string>(),
                ftk::quotes(ffmpegAudioCodecs));
            _cmdLine.ffmpegThreadCount = ftk::CmdLineOption<int>::create(
                { "-ffmpegThreadCount" },
                "Number of threads for I/O.",
                "FFmpeg",
                static_cast<int>(ffmpeg::Options().threadCount));
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_USD)
            _cmdLine.usdRenderWidth = ftk::CmdLineOption<int>::create(
                { "-usdRenderWidth" },
                "Render width.",
                "USD",
                1920);
            _cmdLine.usdComplexity = ftk::CmdLineOption<float>::create(
                { "-usdComplexity" },
                "Render complexity setting.",
                "USD",
                1.F);
            _cmdLine.usdDrawMode = ftk::CmdLineOption<usd::DrawMode>::create(
                { "-usdDrawMode" },
                "Draw mode.",
                "USD",
                usd::DrawMode::ShadedSmooth,
                ftk::quotes(usd::getDrawModeLabels()));
            _cmdLine.usdEnableLighting = ftk::CmdLineOption<bool>::create(
                { "-usdEnableLighting" },
                "Enable lighting.",
                "USD",
                true);
            _cmdLine.usdSRGB = ftk::CmdLineOption<bool>::create(
                { "-usdSRGB" },
                "Enable sRGB color space.",
                "USD",
                true);
            _cmdLine.usdStageCache = ftk::CmdLineOption<int>::create(
                { "-usdStageCache" },
                "Stage cache size.",
                "USD",
                10);
            _cmdLine.usdDiskCache = ftk::CmdLineOption<int>::create(
                { "-usdDiskCache" },
                "Disk cache size in gigabytes. A size of zero disables the cache.",
                "USD",
                0);
#endif // TLRENDER_USD

            IApp::_init(
                context,
                argv,
                "tlbake",
                "Render a timeline to a movie or image sequence.",
                {
                    _cmdLine.input,
                    _cmdLine.output
                },
                {
                    _cmdLine.inOutRange,
                    _cmdLine.renderSize,
                    _cmdLine.outputPixelType,
                    _cmdLine.ocioFileName,
                    _cmdLine.ocioInput,
                    _cmdLine.ocioDisplay,
                    _cmdLine.ocioView,
                    _cmdLine.ocioLook,
                    _cmdLine.lutFileName,
                    _cmdLine.lutOrder,
                    _cmdLine.sequenceDefaultSpeed,
                    _cmdLine.sequenceThreadCount,
#if defined(TLRENDER_EXR)
                    _cmdLine.exrCompression,
                    _cmdLine.exrDWACompressionLevel,
#endif // TLRENDER_EXR
#if defined(TLRENDER_FFMPEG_PLUGIN)
                    _cmdLine.ffmpegCodec,
                    _cmdLine.ffmpegAudioCodec,
                    _cmdLine.ffmpegThreadCount,
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_USD)
                    _cmdLine.usdRenderWidth,
                    _cmdLine.usdComplexity,
                    _cmdLine.usdDrawMode,
                    _cmdLine.usdEnableLighting,
                    _cmdLine.usdSRGB,
                    _cmdLine.usdStageCache,
                    _cmdLine.usdDiskCache,
#endif // TLRENDER_USD
                });
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
            _startTime = std::chrono::steady_clock::now();

            // Create the window.
            _window = ftk::gl::Window::create(
                _context,
                "tlbake",
                ftk::Size2I(1, 1),
                static_cast<int>(ftk::gl::WindowOptions::MakeCurrent));

            // Read the timeline.
            Options options;
            options.ioOptions = _getIOOptions();
            if (_cmdLine.sequenceThreadCount->hasValue())
            {
                // A timeline option now: the timeline is what decodes
                // sequences, so it is what the count applies to.
                options.readThreadCount =
                    _cmdLine.sequenceThreadCount->getValue();
            }
            _timeline = Timeline::create(
                _context,
                _cmdLine.input->getValue(),
                options);
            _timeRange = _timeline->getTimeRange();
            _print(ftk::Format("Timeline range: {0}-{1}").
                arg(_timeRange.start_time().value()).
                arg(_timeRange.end_time_inclusive().value()));
            _print(ftk::Format("Timeline speed: {0}").arg(_timeRange.duration().rate()));

            // Time range.
            if (_cmdLine.inOutRange->hasValue())
            {
                _timeRange = _cmdLine.inOutRange->getValue();
            }
            _print(ftk::Format("In/out range: {0}-{1}").
                arg(_timeRange.start_time().value()).
                arg(_timeRange.end_time_inclusive().value()));
            _inputTime = _timeRange.start_time();
            _outputTime = OTIO_NS::RationalTime(0.0, _timeRange.duration().rate());

            // Render information.
            const auto& info = _timeline->getIOInfo();
            if (info.video.empty())
            {
                throw std::runtime_error("No video to render");
            }
            _renderSize = ftk::Size2I(info.video[0].size.w, info.video[0].size.h);
            if (_cmdLine.renderSize->hasValue())
            {
                _renderSize = _cmdLine.renderSize->getValue();
            }
            _print(ftk::Format("Render size: {0}").arg(_renderSize));

            // Create the renderer.
            _render = gl::Render::create(
                _context->getLogSystem(),
                _context->getSystem<ftk::FontSystem>());
            _buffer = ftk::gl::OffscreenBuffer::create(
                _renderSize,
                ftk::gl::offscreenColorDefault);

            // Create the writer.
            const std::string output = _cmdLine.output->getValue();
            _writerPlugin = _context->getSystem<WriteSystem>()->getPlugin(ftk::Path(output));
            if (!_writerPlugin)
            {
                throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
            }
            _outputInfo.size.w = _renderSize.w;
            _outputInfo.size.h = _renderSize.h;
            _outputInfo.type = info.video[0].type;
            if (_cmdLine.outputPixelType->hasValue())
            {
                _outputInfo.type = _cmdLine.outputPixelType->getValue();
            }
            _outputInfo = _writerPlugin->getInfo(_outputInfo);
            _print(ftk::Format("Output info: {0} {1}").
                arg(_outputInfo.size).
                arg(_outputInfo.type));
            _outputImage = ftk::Image::create(_outputInfo);
            IOInfo ioInfo;
            ioInfo.video.push_back(_outputInfo);
            ioInfo.videoTime = _timeRange;
#if defined(TLRENDER_FFMPEG_PLUGIN)
            if (info.audio.isValid() &&
                std::dynamic_pointer_cast<ffmpeg::WritePlugin>(_writerPlugin))
            {
                _hasAudio = true;
                ioInfo.audio = info.audio;
                ioInfo.audioTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, info.audio.sampleRate),
                    _timeRange.duration().rescaled_to(info.audio.sampleRate));
                _audioStartSeconds = _timeRange.start_time().rescaled_to(1.0).value();
                _audioDurationSeconds = _timeRange.duration().rescaled_to(1.0).value();
                _print(ftk::Format("Audio: {0} channels, {1}Hz").
                    arg(ioInfo.audio.channelCount).
                    arg(ioInfo.audio.sampleRate));
            }
#endif // TLRENDER_FFMPEG_PLUGIN
            _writer = _writerPlugin->write(ftk::Path(output), ioInfo, _getIOOptions());
            if (!_writer)
            {
                throw std::runtime_error(ftk::Format("Cannot open: \"{0}\"").arg(output));
            }

            // Set options.
            if (_cmdLine.ocioFileName->hasValue() ||
                _cmdLine.ocioInput->hasValue() ||
                _cmdLine.ocioDisplay->hasValue() ||
                _cmdLine.ocioView->hasValue() ||
                _cmdLine.ocioLook->hasValue())
            {
                _ocioOptions.enabled = true;
                if (_cmdLine.ocioFileName->hasValue())
                {
                    _ocioOptions.fileName = _cmdLine.ocioFileName->getValue();
                }
                if (_cmdLine.ocioInput->hasValue())
                {
                    _ocioOptions.input = _cmdLine.ocioInput->getValue();
                }
                if (_cmdLine.ocioDisplay->hasValue())
                {
                    _ocioOptions.display = _cmdLine.ocioDisplay->getValue();
                }
                if (_cmdLine.ocioView->hasValue())
                {
                    _ocioOptions.view = _cmdLine.ocioView->getValue();
                }
                if (_cmdLine.ocioLook->hasValue())
                {
                    _ocioOptions.look = _cmdLine.ocioLook->getValue();
                }
            }
            if (_cmdLine.lutFileName->hasValue() ||
                _cmdLine.lutOrder->hasValue())
            {
                _lutOptions.enabled = true;
                if (_cmdLine.lutFileName->hasValue())
                {
                    _lutOptions.fileName = _cmdLine.lutFileName->getValue();
                }
                if (_cmdLine.lutOrder->hasValue())
                {
                    _lutOptions.order = _cmdLine.lutOrder->getValue();
                }
            }

            // Start the main loop.
            while (_running)
            {
                _tick();
            }

            // Finish writing.
            _writer->finish();

            const size_t readErrorCount = _timeline->getReadErrorCount();
            if (readErrorCount > 0)
            {
                throw std::runtime_error(
                    ftk::Format(
                        "{0} error(s) occurred while reading, the output may "
                        "be incomplete: {1}").
                    arg(readErrorCount).
                    arg(_timeline->getReadError()));
            }

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<float> diff = now - _startTime;
            _print(ftk::Format("Seconds elapsed: {0}").arg(diff.count()));
            _print(ftk::Format("Average FPS: {0}").arg(_timeRange.duration().value() / diff.count()));
        }

        IOOptions App::_getIOOptions() const
        {
            IOOptions out;
            if (_cmdLine.sequenceDefaultSpeed->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.sequenceDefaultSpeed->getValue();
                out["SeqIO/DefaultSpeed"] = ss.str();
            }
#if defined(TLRENDER_EXR)
            if (_cmdLine.exrCompression->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.exrCompression->getValue();
                out["OpenEXR/Compression"] = ss.str();
            }
            if (_cmdLine.exrDWACompressionLevel->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.exrDWACompressionLevel->getValue();
                out["OpenEXR/DWACompressionLevel"] = ss.str();
            }
#endif // TLRENDER_EXR
#if defined(TLRENDER_FFMPEG_PLUGIN)
            if (_cmdLine.ffmpegCodec->hasValue())
            {
                out["FFmpeg/Codec"] = _cmdLine.ffmpegCodec->getValue();
            }
            if (_cmdLine.ffmpegAudioCodec->hasValue())
            {
                out["FFmpeg/AudioCodec"] = _cmdLine.ffmpegAudioCodec->getValue();
            }
            if (_cmdLine.ffmpegThreadCount->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.ffmpegThreadCount->getValue();
                out["FFmpeg/ThreadCount"] = ss.str();
            }
#endif // TLRENDER_FFMPEG_PLUGIN

#if defined(TLRENDER_USD)
            if (_cmdLine.usdRenderWidth->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.usdRenderWidth->getValue();
                out["USD/RenderWidth"] = ss.str();
            }
            if (_cmdLine.usdComplexity->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.usdComplexity->getValue();
                out["USD/Complexity"] = ss.str();
            }
            if (_cmdLine.usdDrawMode->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.usdDrawMode->getValue();
                out["USD/DrawMode"] = ss.str();
            }
            if (_cmdLine.usdEnableLighting->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.usdEnableLighting->getValue();
                out["USD/EnableLighting"] = ss.str();
            }
            if (_cmdLine.usdSRGB->hasValue())
            {
                std::stringstream ss;
                ss << _cmdLine.usdSRGB->getValue();
                out["USD/sRGB"] = ss.str();
            }
            if (_cmdLine.usdStageCache->hasValue())
            {
                std::stringstream ss;
                ss << std::max(0, _cmdLine.usdStageCache->getValue());
                out["USD/StageCacheCount"] = ss.str();
            }
            if (_cmdLine.usdDiskCache->hasValue())
            {
                std::stringstream ss;
                ss << std::max(0, _cmdLine.usdDiskCache->getValue()) * ftk::gigabyte;
                out["USD/DiskCacheByteCount"] = ss.str();
            }
#endif // TLRENDER_USD

            return out;
        }

        void App::_tick()
        {
            _context->tick();

            _printProgress();

            // Render the video.
            ftk::gl::OffscreenBufferBinding binding(_buffer);
            _render->begin(_renderSize);
            _render->setOCIOOptions(_ocioOptions);
            _render->setLUTOptions(_lutOptions);
            const auto videoData = _timeline->getVideo(_inputTime).future.get();
            _render->drawVideo(
                { videoData },
                { ftk::Box2I(0, 0, _renderSize.w, _renderSize.h) });
            _render->end();

            // Write the frame.
            glPixelStorei(GL_PACK_ALIGNMENT, _outputInfo.layout.alignment);
#if defined(FTK_API_GL_4_1)
            glPixelStorei(GL_PACK_SWAP_BYTES, _outputInfo.layout.endian != ftk::getEndian());
#endif // FTK_API_GL_4_1
            const GLenum format = ftk::gl::getReadPixelsFormat(_outputInfo.type);
            const GLenum type = ftk::gl::getReadPixelsType(_outputInfo.type);
            if (GL_NONE == format || GL_NONE == type)
            {
                throw std::runtime_error(ftk::Format("Cannot write: \"{0}\"").arg(_cmdLine.output->getValue()));
            }
            glReadPixels(
                0,
                0,
                _outputInfo.size.w,
                _outputInfo.size.h,
                format,
                type,
                _outputImage->getData());
            _writer->writeVideo(_outputTime, _outputImage);

            // Advance the time.
            _inputTime += OTIO_NS::RationalTime(1, _inputTime.rate());
            if (_inputTime > _timeRange.end_time_inclusive())
            {
                _running = false;
            }
            _outputTime += OTIO_NS::RationalTime(1, _outputTime.rate());

            // Write the audio.
            _writeAudio();
        }

        void App::_writeAudio()
        {
            if (!_hasAudio)
                return;

            const double videoSeconds = _outputTime.rescaled_to(1.0).value();
            while (_audioSeconds < std::min(videoSeconds, _audioDurationSeconds))
            {
                // Get one second of audio from the timeline and mix the
                // layers together.
                auto frame = _timeline->getAudio(
                    _audioStartSeconds + _audioSeconds).future.get();
                std::vector<std::shared_ptr<Audio> > layers;
                for (const auto& layer : frame.layers)
                {
                    if (layer.audio)
                    {
                        layers.push_back(layer.audio);
                    }
                }
                auto audio = mixAudio(layers, 1.F);
                if (audio && audio->isValid())
                {
                    // Trim the final chunk to the in/out range.
                    const double remaining = _audioDurationSeconds - _audioSeconds;
                    if (remaining < 1.0)
                    {
                        const size_t sampleCount = std::min(
                            audio->getSampleCount(),
                            static_cast<size_t>(
                                remaining * audio->getInfo().sampleRate + .5));
                        auto tmp = Audio::create(audio->getInfo(), sampleCount);
                        std::memcpy(
                            tmp->getData(),
                            audio->getData(),
                            tmp->getByteCount());
                        audio = tmp;
                    }
                    const OTIO_NS::TimeRange timeRange(
                        OTIO_NS::RationalTime(
                            _audioSamples,
                            audio->getInfo().sampleRate),
                        OTIO_NS::RationalTime(
                            audio->getSampleCount(),
                            audio->getInfo().sampleRate));
                    _writer->writeAudio(timeRange, audio);
                    _audioSamples += audio->getSampleCount();
                }
                _audioSeconds += 1.0;
            }
        }

        void App::_printProgress()
        {
            const int64_t c = static_cast<int64_t>(_inputTime.value() - _timeRange.start_time().value());
            const int64_t d = static_cast<int64_t>(_timeRange.duration().value());
            if (d >= 100 && c % (d / 100) == 0)
            {
                _print(ftk::Format("Complete: {0}%").arg(static_cast<int>(c / static_cast<float>(d) * 100)));
            }
        }
    }
}
