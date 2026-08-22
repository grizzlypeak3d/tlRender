// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/IRender.h>
#include <tlRender/Timeline/Timeline.h>

#include <tlRender/IO/SeqIO.h>
#if defined(TLRENDER_EXR)
#include <tlRender/IO/EXR.h>
#endif // TLRENDER_EXR
#if defined(TLRENDER_FFMPEG_PLUGIN)
#include <tlRender/IO/FFmpeg.h>
#endif // TLRENDER_FFMPEG_PLUGIN

#include <ftk/Core/CmdLine.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/Core/IApp.h>

namespace ftk
{
    namespace gl
    {
        class Window;
    }
}

namespace tl
{
    //! tlbake application
    namespace bake
    {
        //! Application command line.
        struct CmdLine
        {
            std::shared_ptr<ftk::CmdLineArg<std::string> > input;
            std::shared_ptr<ftk::CmdLineArg<std::string> > output;
            std::shared_ptr<ftk::CmdLineOption<OTIO_NS::TimeRange> > inOutRange;
            std::shared_ptr<ftk::CmdLineOption<ftk::Size2I> > renderSize;
            std::shared_ptr<ftk::CmdLineOption<ftk::ImageType> > outputPixelType;
#if defined(TLRENDER_OCIO)
            std::shared_ptr<ftk::CmdLineOption<std::string> > ocioFileName;
            std::shared_ptr<ftk::CmdLineOption<std::string> > ocioInput;
            std::shared_ptr<ftk::CmdLineOption<std::string> > ocioDisplay;
            std::shared_ptr<ftk::CmdLineOption<std::string> > ocioView;
            std::shared_ptr<ftk::CmdLineOption<std::string> > ocioLook;
            std::shared_ptr<ftk::CmdLineOption<std::string> > lutFileName;
            std::shared_ptr<ftk::CmdLineOption<LUTOrder> > lutOrder;
#endif // TLRENDER_OCIO
            std::shared_ptr<ftk::CmdLineOption<double> > sequenceDefaultSpeed;
            std::shared_ptr<ftk::CmdLineOption<int> > sequenceThreadCount;
#if defined(TLRENDER_EXR)
            std::shared_ptr<ftk::CmdLineOption<exr::Compression> > exrCompression;
            std::shared_ptr<ftk::CmdLineOption<float> > exrDWACompressionLevel;
#endif // TLRENDER_EXR
#if defined(TLRENDER_FFMPEG_PLUGIN)
            std::shared_ptr<ftk::CmdLineOption<std::string> > ffmpegCodec;
            std::shared_ptr<ftk::CmdLineOption<std::string> > ffmpegAudioCodec;
            std::shared_ptr<ftk::CmdLineOption<int> > ffmpegThreadCount;
#endif // TLRENDER_FFMPEG_PLUGIN
        };

        //! Application.
        class App : public ftk::IApp
        {
            FTK_NON_COPYABLE(App);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                std::vector<std::string>&);
            App();

        public:
            ~App();

            //! Create a new application.
            static std::shared_ptr<App> create(
                const std::shared_ptr<ftk::Context>&,
                std::vector<std::string>&);

            //! Run the application.
            void run();

        private:
            IOOptions _getIOOptions() const;

            void _tick();
            void _writeAudio();
            void _printProgress();

            CmdLine _cmdLine;
            OCIOOptions _ocioOptions;
            LUTOptions _lutOptions;

            std::shared_ptr<Timeline> _timeline;
            ftk::Size2I _renderSize;
            ftk::ImageInfo _outputInfo;
            OTIO_NS::TimeRange _timeRange;
            OTIO_NS::RationalTime _inputTime;
            OTIO_NS::RationalTime _outputTime;
            bool _hasAudio = false;
            double _audioStartSeconds = 0.0;
            double _audioDurationSeconds = 0.0;
            double _audioSeconds = 0.0;
            int64_t _audioSamples = 0;

            std::shared_ptr<ftk::gl::Window> _window;
            std::shared_ptr<IRender> _render;
            std::shared_ptr<ftk::gl::OffscreenBuffer> _buffer;

            std::shared_ptr<IWritePlugin> _writerPlugin;
            std::shared_ptr<IWrite> _writer;
            //! Whether the writer names its output from the time it is
            //! given, which a sequence does and a movie does not.
            std::shared_ptr<ftk::Image> _outputImage;

            bool _running = true;
            std::chrono::steady_clock::time_point _startTime;
        };
    }
}
