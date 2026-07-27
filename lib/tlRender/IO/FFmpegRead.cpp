// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegReadPrivate.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <algorithm>

extern "C"
{
#include <libavutil/opt.h>

} // extern "C"

namespace tl
{
    namespace ffmpeg
    {
        AVIOBufferData::AVIOBufferData(const uint8_t* p, size_t size) :
            p(p),
            size(size)
        {}

        int avIOBufferRead(void* opaque, uint8_t* buf, int bufSize)
        {
            AVIOBufferData* bufferData = static_cast<AVIOBufferData*>(opaque);

            const int64_t remaining = bufferData->size - bufferData->offset;
            int bufSizeClamped = ftk::clamp(
                static_cast<int64_t>(bufSize),
                static_cast<int64_t>(0),
                remaining);
            if (!bufSizeClamped)
            {
                return AVERROR_EOF;
            }

            memcpy(buf, bufferData->p + bufferData->offset, bufSizeClamped);
            bufferData->offset += bufSizeClamped;

            return bufSizeClamped;
        }

        int64_t avIOBufferSeek(void* opaque, int64_t offset, int whence)
        {
            AVIOBufferData* bufferData = static_cast<AVIOBufferData*>(opaque);

            if (whence & AVSEEK_SIZE)
            {
                return bufferData->size;
            }

            int64_t pos = 0;
            switch (whence & ~AVSEEK_FORCE)
            {
            case SEEK_SET:
                pos = offset;
                break;
            case SEEK_CUR:
                pos = static_cast<int64_t>(bufferData->offset) + offset;
                break;
            case SEEK_END:
                pos = static_cast<int64_t>(bufferData->size) + offset;
                break;
            default:
                return AVERROR(EINVAL);
            }
            if (pos < 0)
            {
                return AVERROR(EINVAL);
            }

            bufferData->offset = ftk::clamp(
                pos,
                static_cast<int64_t>(0),
                static_cast<int64_t>(bufferData->size));

            return static_cast<int64_t>(bufferData->offset);
        }

        void Read::_init(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, mem, options, logSystem);
            FTK_P();

            auto i = options.find("FFmpeg/YUVToRGB");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.yuvToRGBConversion;
            }
            i = options.find("FFmpeg/HWAccel");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.hwAccel;
            }
            i = options.find("FFmpeg/AudioChannelCount");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.audioConvertInfo.channelCount;
            }
            i = options.find("FFmpeg/AudioType");
            if (i != options.end())
            {
                from_string(i->second, p.options.audioConvertInfo.type);
            }
            i = options.find("FFmpeg/AudioSampleRate");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.audioConvertInfo.sampleRate;
            }
            i = options.find("FFmpeg/ThreadCount");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.threadCount;
            }
            i = options.find("FFmpeg/RequestTimeout");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.requestTimeout;
            }
            i = options.find("FFmpeg/VideoBufferSize");
            if (i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> p.options.videoBufferSize;
            }
            i = options.find("FFmpeg/AudioBufferSize");
            if (i != options.end())
            {
                from_string(i->second, p.options.audioBufferSize);
            }

            p.videoThread.thread = std::thread(
                [this, path]
                {
                    FTK_P();
                    try
                    {
                        p.readVideo = std::make_shared<ReadVideo>(
                            path.hasProtocol() ? path.get() : path.getFileName(true),
                            _mem,
                            p.options,
                            _logSystem.lock());
                        const auto& videoInfo = p.readVideo->getInfo();
                        if (videoInfo.isValid())
                        {
                            p.info.video.push_back(videoInfo);
                            p.info.videoTime = p.readVideo->getTimeRange();
                            p.info.tags = p.readVideo->getTags();
                        }

                        p.readAudio = std::make_shared<ReadAudio>(
                            path.hasProtocol() ? path.get() : path.getFileName(true),
                            _mem,
                            p.info.videoTime.duration().rate(),
                            p.options);
                        p.info.audio = p.readAudio->getInfo();
                        p.info.audioTime = p.readAudio->getTimeRange();
                        for (const auto& tag : p.readAudio->getTags())
                        {
                            p.info.tags[tag.first] = tag.second;
                        }

                        if (!p.info.audio.isValid())
                        {
                            // The file has no audio, which most plates do
                            // not. Stopping the queue makes a request for
                            // audio come back empty at once, so nothing
                            // waits on a thread that would have no work.
                            p.audioCondition.stopQueues();
                        }
                        else
                        {
                        p.audioThread.thread = std::thread(
                            [this, path]
                            {
                                FTK_P();
                                try
                                {
                                    _audioThread();
                                }
                                catch (const std::exception& e)
                                {
                                    if (auto logSystem = _logSystem.lock())
                                    {
                                        logSystem->print(
                                            "tl::ffmpeg::Read",
                                            e.what(),
                                            ftk::LogType::Error);
                                    }
                                    std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                                    ++p.errorMutex.audioCount;
                                    if (p.errorMutex.error.empty())
                                    {
                                        p.errorMutex.error = e.what();
                                    }
                                }

                                // The epilogue; the video thread epilogue
                                // only runs at shutdown or if setup fails
                                // before this thread is spawned.
                                p.audioCondition.stopQueues();
                            });
                        }

                        _videoThread();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::ffmpeg::Read",
                                e.what(),
                                ftk::LogType::Error);
                        }
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        ++p.errorMutex.videoCount;
                        if (p.errorMutex.error.empty())
                        {
                            p.errorMutex.error = e.what();
                        }
                    }

                    // The epilogue; the audio queues are included for the
                    // case where setup fails before the audio thread is
                    // spawned.
                    p.videoCondition.stopQueues();
                    p.audioCondition.stopQueues();
                });
        }

        Read::Read() :
            _p(new Private)
        {}

        Read::~Read()
        {
            FTK_P();

            // Stop the conditions and wake the threads so that shutdown
            // does not have to wait for the request timeout.
            p.videoCondition.stop();
            p.audioCondition.stop();
            if (p.videoThread.thread.joinable())
            {
                p.videoThread.thread.join();
            }
            if (p.audioThread.thread.joinable())
            {
                p.audioThread.thread.join();
            }
        }

        std::shared_ptr<Read> Read::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Read>(new Read);
            out->_init(path, {}, options, logSystem);
            return out;
        }

        std::shared_ptr<Read> Read::create(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Read>(new Read);
            out->_init(path, mem, options, logSystem);
            return out;
        }

        std::future<IOInfo> Read::getInfo()
        {
            FTK_P();
            return p.infoRequests.push(std::make_shared<Private::InfoRequest>());
        }

        std::future<VideoData> Read::readVideo(
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            FTK_P();
            auto request = std::make_shared<Private::VideoRequest>();
            request->time = time;
            request->options = merge(options, _options);
            return p.videoRequests.push(request);
        }

        std::future<AudioData> Read::readAudio(
            const OTIO_NS::TimeRange& timeRange,
            const IOOptions& options)
        {
            FTK_P();
            auto request = std::make_shared<Private::AudioRequest>();
            request->timeRange = timeRange;
            request->options = merge(options, _options);
            return p.audioRequests.push(request);
        }

        void Read::cancelRequests()
        {
            FTK_P();
            p.infoRequests.cancel();
            p.videoRequests.cancel();
            p.audioRequests.cancel();
        }

        std::string Read::getError() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.error;
        }

        size_t Read::getErrorCount() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.videoCount + p.errorMutex.audioCount;
        }

        void Read::_videoThread()
        {
            FTK_P();
            p.videoThread.currentTime = p.info.videoTime.start_time();
            p.readVideo->start();
            p.videoThread.logTimer = std::chrono::steady_clock::now();
            size_t errorCount = 0;
            while (p.videoCondition.wait(std::chrono::milliseconds(p.options.requestTimeout)))
            {
                // Information requests.
                for (const auto& request : p.infoRequests.popAll())
                {
                    request->promise.set_value(p.info);
                }

                // Video request. The guard completes the promise if an
                // exception escapes; see PromiseGuard.
                if (auto videoRequest = p.videoRequests.pop())
                {
                    PromiseGuard<VideoData> guard(videoRequest->promise);

                    // Seek.
                    if (!videoRequest->time.strictly_equal(p.videoThread.currentTime))
                    {
                        p.videoThread.currentTime = videoRequest->time;
                        p.readVideo->seek(p.videoThread.currentTime);
                    }

                    // Process.
                    while (
                        p.readVideo->isBufferEmpty() &&
                        p.readVideo->isValid() &&
                        p.readVideo->process(p.videoThread.currentTime))
                        ;

                    // Handle the request.
                    VideoData data;
                    data.time = videoRequest->time;
                    if (!p.readVideo->isBufferEmpty())
                    {
                        data.image = p.readVideo->popBuffer();
                    }
                    guard.setValue(std::move(data));

                    p.videoThread.currentTime += OTIO_NS::RationalTime(1.0, p.info.videoTime.duration().rate());
                }

                // Record any new errors from the worker, logging the
                // first one.
                if (p.readVideo->getErrorCount() != errorCount)
                {
                    const bool first = 0 == errorCount;
                    errorCount = p.readVideo->getErrorCount();
                    {
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        p.errorMutex.videoCount = errorCount;
                        if (p.errorMutex.error.empty())
                        {
                            p.errorMutex.error = p.readVideo->getErrorString();
                        }
                    }
                    if (first)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::ffmpeg::Read",
                                ftk::Format("Errors reading video: \"{0}\": {1}").
                                    arg(_path.get()).
                                    arg(p.readVideo->getErrorString()),
                                ftk::LogType::Error);
                        }
                    }
                }

                // Logging.
                /*{
                    const auto now = std::chrono::steady_clock::now();
                    const std::chrono::duration<float> diff = now - p.videoThread.logTimer;
                    if (diff.count() > 10.F)
                    {
                        p.videoThread.logTimer = now;
                        if (auto logSystem = _logSystem.lock())
                        {
                            const std::string id = ftk::Format("tl::ffmpeg::Read {0}").arg(this);
                            logSystem->print(id, ftk::Format(
                                "\n"
                                "    * Path: {0}\n"
                                "    * Video requests: {1}").
                                arg(_path.get()).
                                arg(p.videoRequests.size()));
                        }
                    }
                }*/
            }
        }

        void Read::_audioThread()
        {
            FTK_P();
            p.audioThread.currentTime = p.info.audioTime.start_time();
            p.readAudio->start();
            p.audioThread.logTimer = std::chrono::steady_clock::now();
            const bool audioValid = p.info.audio.isValid();
            size_t errorCount = 0;
            while (p.audioCondition.wait(std::chrono::milliseconds(p.options.requestTimeout)))
            {
                // Audio request. The guard completes the promise if an
                // exception escapes; see PromiseGuard.
                if (auto request = p.audioRequests.pop())
                {
                    PromiseGuard<AudioData> guard(request->promise);

                    size_t requestSampleCount = 0;
                    bool seek = false;
                    if (audioValid)
                    {
                        requestSampleCount = request->timeRange.duration().rescaled_to(p.info.audio.sampleRate).value();
                        if (!request->timeRange.start_time().strictly_equal(p.audioThread.currentTime))
                        {
                            seek = true;
                            p.audioThread.currentTime = request->timeRange.start_time();
                        }
                    }

                    // Seek.
                    if (seek)
                    {
                        p.readAudio->seek(p.audioThread.currentTime);
                    }

                    // Process.
                    bool intersects = false;
                    if (audioValid)
                    {
                        intersects = request->timeRange.intersects(p.info.audioTime);
                    }
                    while (
                        intersects &&
                        p.readAudio->getBufferSize() < request->timeRange.duration().rescaled_to(p.info.audio.sampleRate).value() &&
                        p.readAudio->isValid() &&
                        p.readAudio->process(
                            p.audioThread.currentTime,
                            requestSampleCount ?
                            requestSampleCount :
                            p.options.audioBufferSize.rescaled_to(p.info.audio.sampleRate).value()))
                        ;

                    // Handle the request.
                    AudioData audioData;
                    audioData.time = request->timeRange.start_time();
                    if (audioValid)
                    {
                        // Note that the request time range may be expressed
                        // at any rate, so sizes and offsets must be
                        // rescaled to the sample rate rather than using the
                        // raw time values.
                        audioData.audio = Audio::create(
                            p.info.audio,
                            requestSampleCount);
                        audioData.audio->zero();
                        if (intersects)
                        {
                            size_t offset = 0;
                            if (audioData.time < p.info.audioTime.start_time())
                            {
                                offset = std::min(
                                    static_cast<size_t>(
                                        (p.info.audioTime.start_time() - audioData.time).
                                            rescaled_to(p.info.audio.sampleRate).value()),
                                    requestSampleCount);
                            }
                            p.readAudio->bufferCopy(
                                audioData.audio->getData() + offset * p.info.audio.getByteCount(),
                                audioData.audio->getSampleCount() - offset);
                        }
                    }
                    guard.setValue(std::move(audioData));

                    p.audioThread.currentTime += request->timeRange.duration();
                }

                // Record any new errors from the worker, logging the
                // first one.
                if (p.readAudio->getErrorCount() != errorCount)
                {
                    const bool first = 0 == errorCount;
                    errorCount = p.readAudio->getErrorCount();
                    {
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        p.errorMutex.audioCount = errorCount;
                        if (p.errorMutex.error.empty())
                        {
                            p.errorMutex.error = p.readAudio->getErrorString();
                        }
                    }
                    if (first)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::ffmpeg::Read",
                                ftk::Format("Errors reading audio: \"{0}\": {1}").
                                    arg(_path.get()).
                                    arg(p.readAudio->getErrorString()),
                                ftk::LogType::Error);
                        }
                    }
                }

                // Logging.
                /*{
                    const auto now = std::chrono::steady_clock::now();
                    const std::chrono::duration<float> diff = now - p.audioThread.logTimer;
                    if (diff.count() > 10.F)
                    {
                        p.audioThread.logTimer = now;
                        if (auto logSystem = _logSystem.lock())
                        {
                            const std::string id = ftk::Format("tl::ffmpeg::Read {0}").arg(this);
                            logSystem->print(id, ftk::Format(
                                "\n"
                                "    * Path: {0}\n"
                                "    * Audio requests: {1}").
                                arg(_path.get()).
                                arg(p.audioRequests.size()));
                        }
                    }
                }*/
            }
        }
    }
}
