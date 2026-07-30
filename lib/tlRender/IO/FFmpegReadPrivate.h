// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpegPrivate.h>

#include <tlRender/IO/RequestQueuePrivate.h>

#include <ftk/Core/LogSystem.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

} // extern "C"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace tl
{
    namespace ffmpeg
    {
        struct AVIOBufferData
        {
            AVIOBufferData() = default;
            AVIOBufferData(const uint8_t* p, size_t size);

            const uint8_t* p = nullptr;
            size_t size = 0;
            size_t offset = 0;
        };

        int avIOBufferRead(void* opaque, uint8_t* buf, int bufSize);
        int64_t avIOBufferSeek(void* opaque, int64_t offset, int whence);

        const size_t avIOContextBufferSize = 4096;

        struct ReadOptions
        {
            bool yuvToRGBConversion = false;
            bool hwAccel = false;
            AudioInfo audioConvertInfo;
            size_t threadCount = Options().threadCount;
            size_t videoBufferSize = 4;
            OTIO_NS::RationalTime audioBufferSize = OTIO_NS::RationalTime(2.0, 1.0);
        };

        //! Parse the reader options.
        ReadOptions getReadOptions(const IOOptions&);

        //! Find the stream of the given type to read, or -1. A stream
        //! marked as the default is preferred over the first one found.
        int findStream(AVFormatContext*, AVMediaType);

        class ReadVideo
        {
        public:
            ReadVideo(
                const std::string& fileName,
                const std::vector<ftk::MemFile>& memory,
                const ReadOptions& options,
                const std::shared_ptr<ftk::LogSystem>& logSystem);

            ~ReadVideo();

            bool isValid() const;
            const ftk::ImageInfo& getInfo() const;
            const OTIO_NS::TimeRange& getTimeRange() const;
            const ftk::ImageTags& getTags() const;

            void start();
            void seek(const OTIO_NS::RationalTime&);
            bool process(const OTIO_NS::RationalTime& currentTime);

            //! The number of read/decode errors encountered, and the
            //! first error. Only accessed from the owning thread.
            size_t getErrorCount() const;
            const std::string& getErrorString() const;

            bool isBufferEmpty() const;
            std::shared_ptr<ftk::Image> popBuffer();

        private:
            int _decode(const OTIO_NS::RationalTime& currentTime);
            void _copy(const std::shared_ptr<ftk::Image>&, AVFrame* frame);
            void _setError(int);
            void _initHwAccel(const AVCodec*);
            void _initSws(AVPixelFormat srcFormat);
            static AVPixelFormat _getHwFormat(AVCodecContext*, const AVPixelFormat*);
            void _log(const std::string&, ftk::LogType = ftk::LogType::Message) const;
            void _close();

            std::string _fileName;
            ReadOptions _options;
            ftk::ImageInfo _info;
            OTIO_NS::TimeRange _timeRange;
            ftk::ImageTags _tags;

            AVFormatContext* _avFormatContext = nullptr;
            AVIOBufferData _avIOBufferData;
            uint8_t* _avIOContextBuffer = nullptr;
            AVIOContext* _avIOContext = nullptr;
            AVRational _avSpeed = { 24, 1 };
            int _avStream = -1;
            std::map<int, AVCodecParameters*> _avCodecParameters;
            std::map<int, AVCodecContext*> _avCodecContext;
            AVFrame* _avFrame = nullptr;
            AVFrame* _avFrame2 = nullptr;
            AVPixelFormat _avInputPixelFormat = AV_PIX_FMT_NONE;
            AVPixelFormat _avOutputPixelFormat = AV_PIX_FMT_NONE;
            SwsContext* _swsContext = nullptr;
            AVBufferRef* _hwDeviceContext = nullptr;
            AVPixelFormat _hwPixelFormat = AV_PIX_FMT_NONE;
            AVFrame* _swFrame = nullptr;
            bool _hwAccel = false;
            bool _hwLogged = false;
            std::weak_ptr<ftk::LogSystem> _logSystem;
            std::list<std::shared_ptr<ftk::Image> > _buffer;
            bool _eof = false;
            size_t _errorCount = 0;
            std::string _errorString;
        };

        class ReadAudio
        {
        public:
            ReadAudio(
                const std::string& fileName,
                const std::vector<ftk::MemFile>&,
                const ReadOptions&);

            ~ReadAudio();

            bool isValid() const;
            const AudioInfo& getInfo() const;
            const OTIO_NS::TimeRange& getTimeRange() const;
            const ftk::ImageTags& getTags() const;

            void start();
            void seek(const OTIO_NS::RationalTime&);
            bool process(
                const OTIO_NS::RationalTime& currentTime,
                size_t sampleCount);

            //! The number of read/decode errors encountered, and the
            //! first error. Only accessed from the owning thread.
            size_t getErrorCount() const;
            const std::string& getErrorString() const;

            size_t getBufferSize() const;
            void bufferCopy(uint8_t*, size_t sampleCount);

        private:
            int _decode(const OTIO_NS::RationalTime& currentTime);
            void _setError(int);
            void _close();

            std::string _fileName;
            ReadOptions _options;
            AudioInfo _info;
            OTIO_NS::TimeRange _timeRange;
            ftk::ImageTags _tags;

            AVFormatContext* _avFormatContext = nullptr;
            AVIOBufferData _avIOBufferData;
            uint8_t* _avIOContextBuffer = nullptr;
            AVIOContext* _avIOContext = nullptr;
            int _avStream = -1;
            std::map<int, AVCodecParameters*> _avCodecParameters;
            std::map<int, AVCodecContext*> _avCodecContext;
            AVFrame* _avFrame = nullptr;
            SwrContext* _swrContext = nullptr;
            std::list<std::shared_ptr<Audio> > _buffer;
            bool _eof = false;
            size_t _errorCount = 0;
            std::string _errorString;
        };

        // Errors are recorded by the worker thread and read through
        // getError()/getErrorCount() from any thread.
        struct ErrorMutex
        {
            std::string error;
            size_t count = 0;
            std::mutex mutex;
        };

        struct VideoRead::Private
        {
            ReadOptions options;

            std::shared_ptr<ReadVideo> readVideo;

            IOInfo info;
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct VideoRequest
            {
                OTIO_NS::RationalTime time;
                IOOptions options;
                std::promise<VideoData> promise;
            };
            // The info and video queues share one condition so that the
            // thread can wait for a request on either.
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<VideoRequest, VideoData> videoRequests{ condition };

            std::thread thread;
            // Only accessed from the thread above.
            OTIO_NS::RationalTime currentTime;
            std::chrono::steady_clock::time_point logTimer;

            ErrorMutex errorMutex;
        };

        struct AudioRead::Private
        {
            ReadOptions options;

            std::shared_ptr<ReadAudio> readAudio;

            IOInfo info;
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct AudioRequest
            {
                OTIO_NS::TimeRange timeRange;
                IOOptions options;
                std::promise<AudioData> promise;
            };
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<AudioRequest, AudioData> audioRequests{ condition };

            std::thread thread;
            // Only accessed from the thread above.
            OTIO_NS::RationalTime currentTime;
            std::chrono::steady_clock::time_point logTimer;

            ErrorMutex errorMutex;
        };
    }
}
