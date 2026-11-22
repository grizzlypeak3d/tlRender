// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegReadPrivate.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/OS.h>

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C"
{
#include <libavutil/opt.h>

} // extern "C"

namespace tl
{
    namespace ffmpeg
    {
        namespace
        {
            // How long a reader has to have been left before it is asked
            // for its decoder: a playing reader is idle between frames,
            // and closing it there would have it open its file again for
            // the next one.
            const std::chrono::milliseconds decoderIdleMin(500);

            // How long a reader waits for room with nothing closing
            // before it opens its decoder anyway: as many readers in use
            // as the limit cannot be asked, and a frame late is better
            // than none.
            const std::chrono::milliseconds decoderWaitMax(1000);
        }

        DecoderLimit::DecoderLimit()
        {
            // One for every two gigabytes: four on an eight gigabyte iPad,
            // where a dozen 4K decoders took more than the system allows
            // an application, and sixteen at most.
            const size_t ramGB = ftk::getSysInfo().ramGB;
            _max = ramGB > 0 ?
                std::clamp(ramGB / 2, static_cast<size_t>(2), static_cast<size_t>(16)) :
                8;
        }

        DecoderLimit& DecoderLimit::get()
        {
            static DecoderLimit limit;
            return limit;
        }

        size_t DecoderLimit::getMax() const
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _max;
        }

        void DecoderLimit::setMax(size_t value)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _max = value;
            }
            _cv.notify_all();
        }

        size_t DecoderLimit::getCount() const
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _entries.size();
        }

        void DecoderLimit::open(const void* id, const std::function<void()>& ask)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto i = std::find_if(
                _entries.begin(),
                _entries.end(),
                [id](const Entry& entry) { return entry.id == id; });
            if (i != _entries.end())
            {
                _entries.erase(i);
            }
            if (_max > 0 && _entries.size() >= _max)
            {
                auto deadline = std::chrono::steady_clock::now() + decoderWaitMax;
                size_t closed = _closed;
                while (_max > 0 && _entries.size() >= _max)
                {
                    const auto now = std::chrono::steady_clock::now();
                    if (_closed != closed)
                    {
                        // Room is being made: wait on.
                        closed = _closed;
                        deadline = now + decoderWaitMax;
                    }
                    else if (now >= deadline)
                    {
                        break;
                    }

                    // Ask the idle readers, the one used least lately
                    // first, for as many decoders as there are too many.
                    const size_t over = _entries.size() + 1 - _max;
                    size_t asked = 0;
                    for (const auto& entry : _entries)
                    {
                        if (entry.asked)
                        {
                            ++asked;
                        }
                    }
                    for (auto& entry : _entries)
                    {
                        if (asked >= over)
                        {
                            break;
                        }
                        if (!entry.busy && !entry.asked && now - entry.used >= decoderIdleMin)
                        {
                            entry.asked = true;
                            entry.ask();
                            ++asked;
                        }
                    }

                    // Looked at again as readers finish, and as they come
                    // to have been idle long enough to ask.
                    _cv.wait_until(lock, std::min(deadline, now + std::chrono::milliseconds(100)));
                }
            }
            Entry entry;
            entry.id = id;
            entry.used = std::chrono::steady_clock::now();
            entry.ask = ask;
            _entries.push_back(entry);
        }

        void DecoderLimit::setBusy(const void* id, bool value)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto i = std::find_if(
                    _entries.begin(),
                    _entries.end(),
                    [id](const Entry& entry) { return entry.id == id; });
                if (i != _entries.end())
                {
                    i->busy = value;
                    i->used = std::chrono::steady_clock::now();
                    if (value)
                    {
                        // In use again: no longer the one to ask, and the
                        // last in line to be.
                        i->asked = false;
                        _entries.splice(_entries.end(), _entries, i);
                    }
                }
            }
            _cv.notify_all();
        }

        void DecoderLimit::close(const void* id)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto i = std::find_if(
                    _entries.begin(),
                    _entries.end(),
                    [id](const Entry& entry) { return entry.id == id; });
                if (i != _entries.end())
                {
                    _entries.erase(i);
                    ++_closed;
                }
            }
            _cv.notify_all();
        }

        size_t getDecoderMax()
        {
            return DecoderLimit::get().getMax();
        }

        void setDecoderMax(size_t value)
        {
            DecoderLimit::get().setMax(value);
        }

        size_t getDecoderCount()
        {
            return DecoderLimit::get().getCount();
        }

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

        ReadOptions getReadOptions(const IOOptions& options)
        {
            ReadOptions out;
            if (auto i = options.find("FFmpeg/YUVToRGB"); i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.yuvToRGBConversion;
            }
            if (auto i = options.find("Texture"); i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.textures;
            }
            if (auto i = options.find("FFmpeg/HWAccel"); i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.hwAccel;
            }
            if (auto i = options.find("FFmpeg/AudioChannelCount");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.audioConvertInfo.channelCount;
            }
            if (auto i = options.find("FFmpeg/AudioType"); i != options.end())
            {
                from_string(i->second, out.audioConvertInfo.type);
            }
            if (auto i = options.find("FFmpeg/AudioSampleRate");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.audioConvertInfo.sampleRate;
            }
            if (auto i = options.find("FFmpeg/AudioMerge");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.audioMerge;
            }
            if (auto i = options.find("FFmpeg/ThreadCount");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.threadCount;
            }
            if (auto i = options.find("FFmpeg/VideoBufferSize");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> out.videoBufferSize;
            }
            if (auto i = options.find("FFmpeg/AudioBufferSize");
                i != options.end())
            {
                from_string(i->second, out.audioBufferSize);
            }
            return out;
        }

        int findStream(AVFormatContext* avFormatContext, AVMediaType type)
        {
            int out = -1;
            for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
            {
                if (type == avFormatContext->streams[i]->codecpar->codec_type &&
                    AV_DISPOSITION_DEFAULT == avFormatContext->streams[i]->disposition)
                {
                    out = i;
                    break;
                }
            }
            if (-1 == out)
            {
                for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
                {
                    if (type == avFormatContext->streams[i]->codecpar->codec_type)
                    {
                        out = i;
                        break;
                    }
                }
            }
            return out;
        }

        namespace
        {
            //! The file name to hand to a worker; a path with a protocol is
            //! opened by FFmpeg itself.
            std::string getFileName(const ftk::Path& path)
            {
                return path.hasProtocol() ? path.get() : path.getFileName(true);
            }
        }

        void VideoRead::_init(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, mem, options, logSystem);
            FTK_P();

            p.options = getReadOptions(options);

            p.thread = std::thread(
                [this, path]
                {
                    FTK_P();
                    ftk::setCurrentThreadName("tl:ffmpeg-video");
                    try
                    {
                        p.fileName = getFileName(path);
                        _decoderOpen();
                        p.readVideo = std::make_shared<ReadVideo>(
                            p.fileName,
                            _mem,
                            p.options,
                            _logSystem.lock());
                        const auto& videoInfo = p.readVideo->getInfo();
                        if (videoInfo.isValid())
                        {
                            p.info.video.push_back(videoInfo);
                            p.info.videoTime = p.readVideo->getTimeRange();
                            p.info.videoSource = p.readVideo->getSource();
                            p.info.tags = p.readVideo->getTags();
                        }
                        p.infoValid = true;
                        DecoderLimit::get().setBusy(this, false);

                        _run();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::ffmpeg::VideoRead",
                                e.what(),
                                ftk::LogType::Error);
                        }
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        ++p.errorMutex.count;
                        if (p.errorMutex.error.empty())
                        {
                            p.errorMutex.error = e.what();
                        }
                    }

                    // The epilogue.
                    p.readVideo.reset();
                    DecoderLimit::get().close(this);
                    p.condition.stopQueues();
                });
        }

        void VideoRead::_decoderOpen()
        {
            FTK_P();
            p.decoderAsked = false;
            DecoderLimit::get().open(
                this,
                [this]
                {
                    _p->decoderAsked = true;
                    _p->condition.wake();
                });
        }

        VideoRead::VideoRead() :
            _p(new Private)
        {}

        VideoRead::~VideoRead()
        {
            FTK_P();

            // Stop the condition and wake the thread so that shutdown does
            // not have to wait for the request timeout.
            p.condition.stop();
            if (p.thread.joinable())
            {
                p.thread.join();
            }
        }

        std::shared_ptr<VideoRead> VideoRead::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<VideoRead>(new VideoRead);
            out->_init(path, {}, options, logSystem);
            return out;
        }

        std::shared_ptr<VideoRead> VideoRead::create(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<VideoRead>(new VideoRead);
            out->_init(path, mem, options, logSystem);
            return out;
        }

        std::future<IOInfo> VideoRead::getInfo()
        {
            FTK_P();
            if (p.infoValid)
            {
                std::promise<IOInfo> promise;
                promise.set_value(p.info);
                return promise.get_future();
            }
            return p.infoRequests.push(std::make_shared<Private::InfoRequest>());
        }

        std::future<VideoData> VideoRead::readVideo(
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            FTK_P();
            auto request = std::make_shared<Private::VideoRequest>();
            request->time = time;
            request->options = merge(options, _options);
            return p.videoRequests.push(request);
        }

        void VideoRead::setPlayhead(const std::optional<Playhead>& value)
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.playheadMutex);
            p.playhead = value;
        }

        void VideoRead::cancelRequests()
        {
            FTK_P();
            p.infoRequests.cancel();
            p.videoRequests.cancel();
        }

        std::string VideoRead::getError() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.error;
        }

        size_t VideoRead::getErrorCount() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.count;
        }

        void VideoRead::_run()
        {
            FTK_P();
            // Fixed once the file is probed, so it is read here rather than
            // per request. A file with no video stream reads nothing, so the
            // empty range it falls back to is never used.
            const OTIO_NS::TimeRange videoTime =
                p.info.videoTime.value_or(OTIO_NS::TimeRange());
            p.currentTime = videoTime.start_time();
            p.readVideo->start();
            size_t errorCount = 0;
            // A reader nobody has asked anything of for a while closes
            // its file and decoder, and opens them again on the next
            // request: a decoder, and a hardware decoder's session above
            // all, is a good deal of memory to keep for a clip the
            // playhead left behind, and a timeline keeps readers for
            // every clip it has played lately. What was learned about
            // the file the first time is kept.
            const std::chrono::seconds idleTimeout(5);
            auto lastRequest = std::chrono::steady_clock::now();
            while (p.condition.waitFor(std::chrono::seconds(1)))
            {
                // Information requests.
                for (const auto& request : p.infoRequests.popAll())
                {
                    request->promise.set_value(p.info);
                }

                // Video request. The guard completes the promise if an
                // exception escapes; see PromiseGuard.
                std::optional<Playhead> playhead;
                {
                    std::unique_lock<std::mutex> lock(p.playheadMutex);
                    playhead = p.playhead;
                }
                std::shared_ptr<Private::VideoRequest> videoRequest;
                if (playhead.has_value() && playhead->playing)
                {
                    // The playhead is moving. The frames it has passed are
                    // declined rather than decoded (VideoData::declined; the
                    // player drops them and asks again when it wants them),
                    // and the next frame decoded is the first in line that
                    // can still reach the screen, by the request time
                    // measured below; failing that, the furthest ahead. A
                    // reader slower than the playhead otherwise decoded
                    // every frame just after it was needed, and one frame in
                    // three of what it decoded was seen.
                    const double rate = videoTime.duration().rate();
                    const auto ahead = [&playhead, rate](const OTIO_NS::RationalTime& time)
                    {
                        const double d =
                            time.rescaled_to(rate).value() -
                            playhead->time.rescaled_to(rate).value();
                        return playhead->reverse ? -d : d;
                    };
                    for (const auto& request : p.videoRequests.popIf(
                        [&ahead](const std::shared_ptr<Private::VideoRequest>& request)
                        {
                            return ahead(request->time) < 0.0;
                        }))
                    {
                        VideoData data;
                        data.time = request->time;
                        data.declined = true;
                        request->promise.set_value(std::move(data));
                    }
                    const double margin = std::ceil(p.requestSeconds * rate + .5);
                    videoRequest = p.videoRequests.pop(
                        [&ahead, margin](const std::list<std::shared_ptr<Private::VideoRequest> >& requests)
                        {
                            auto out = requests.end();
                            double furthest = -1.0;
                            for (auto i = requests.begin(); i != requests.end(); ++i)
                            {
                                const double a = ahead((*i)->time);
                                if (a >= margin)
                                {
                                    out = i;
                                    break;
                                }
                                if (a > furthest)
                                {
                                    furthest = a;
                                    out = i;
                                }
                            }
                            return out;
                        });
                }
                else
                {
                    videoRequest = p.videoRequests.pop();
                }
                if (videoRequest)
                {
                    PromiseGuard<VideoData> guard(videoRequest->promise);
                    lastRequest = std::chrono::steady_clock::now();
                    if (!p.readVideo)
                    {
                        _decoderOpen();
                        p.readVideo = std::make_shared<ReadVideo>(
                            p.fileName,
                            _mem,
                            p.options,
                            _logSystem.lock());
                        p.readVideo->start();
                        p.currentTime = videoTime.start_time();
                        errorCount = 0;
                    }
                    else
                    {
                        p.decoderAsked = false;
                        DecoderLimit::get().setBusy(this, true);
                    }

                    // Seek.
                    if (!videoRequest->time.strictly_equal(p.currentTime))
                    {
                        p.currentTime = videoRequest->time;
                        p.readVideo->seek(p.currentTime);
                    }

                    // Process, until a frame at or after the time is
                    // buffered: the one for it, by the frames' times,
                    // whichever way the reader got here.
                    p.readVideo->dropBefore(p.currentTime);
                    while (
                        p.readVideo->isBufferEmpty() &&
                        p.readVideo->isValid() &&
                        p.readVideo->process(p.currentTime))
                        ;

                    // Handle the request.
                    VideoData data;
                    data.time = videoRequest->time;
                    data.image = p.readVideo->frameFor(p.currentTime);
                    guard.setValue(std::move(data));

                    const double seconds = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - lastRequest).count();
                    p.requestSeconds = p.requestSeconds > 0.0 ?
                        p.requestSeconds * .8 + seconds * .2 :
                        seconds;

                    p.currentTime += OTIO_NS::RationalTime(1.0, videoTime.duration().rate());
                    DecoderLimit::get().setBusy(this, false);
                }
                else if (p.readVideo &&
                    (p.decoderAsked ||
                    std::chrono::steady_clock::now() - lastRequest > idleTimeout))
                {
                    // Idle for a while, or asked for the decoder by the
                    // limit for another reader.
                    p.readVideo.reset();
                    p.decoderAsked = false;
                    DecoderLimit::get().close(this);
                }

                // Record any new errors from the worker, logging the
                // first one.
                if (p.readVideo && p.readVideo->getErrorCount() != errorCount)
                {
                    const bool first = 0 == errorCount;
                    errorCount = p.readVideo->getErrorCount();
                    {
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        p.errorMutex.count = errorCount;
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
                                "tl::ffmpeg::VideoRead",
                                ftk::Format("Errors reading video: \"{0}\": {1}").
                                    arg(_path.get()).
                                    arg(p.readVideo->getErrorString()),
                                ftk::LogType::Error);
                        }
                    }
                }
            }
        }

        void AudioRead::_init(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, mem, options, logSystem);
            FTK_P();

            p.options = getReadOptions(options);

            p.thread = std::thread(
                [this, path]
                {
                    FTK_P();
                    ftk::setCurrentThreadName("tl:ffmpeg-audio");
                    try
                    {
                        p.readAudio = std::make_shared<ReadAudio>(
                            getFileName(path),
                            _mem,
                            p.options);
                        p.info.audio = p.readAudio->getInfo();
                        p.info.audioTime = p.readAudio->getTimeRange();
                        p.info.audioSource = p.readAudio->getSource();
                        p.info.tags = p.readAudio->getTags();

                        if (!p.info.audio.isValid())
                        {
                            // The file has no audio, which most plates do
                            // not. Stopping the queue makes a request for
                            // audio come back empty at once, so nothing
                            // waits on work that will never be done; the
                            // thread stays to serve information requests.
                            p.audioRequests.stop();
                        }
                        p.infoValid = true;

                        _run();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::ffmpeg::AudioRead",
                                e.what(),
                                ftk::LogType::Error);
                        }
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        ++p.errorMutex.count;
                        if (p.errorMutex.error.empty())
                        {
                            p.errorMutex.error = e.what();
                        }
                    }

                    // The epilogue.
                    p.condition.stopQueues();
                });
        }

        AudioRead::AudioRead() :
            _p(new Private)
        {}

        AudioRead::~AudioRead()
        {
            FTK_P();

            // Stop the condition and wake the thread so that shutdown does
            // not have to wait for the request timeout.
            p.condition.stop();
            if (p.thread.joinable())
            {
                p.thread.join();
            }
        }

        std::shared_ptr<AudioRead> AudioRead::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<AudioRead>(new AudioRead);
            out->_init(path, {}, options, logSystem);
            return out;
        }

        std::shared_ptr<AudioRead> AudioRead::create(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<AudioRead>(new AudioRead);
            out->_init(path, mem, options, logSystem);
            return out;
        }

        std::future<IOInfo> AudioRead::getInfo()
        {
            FTK_P();
            if (p.infoValid)
            {
                std::promise<IOInfo> promise;
                promise.set_value(p.info);
                return promise.get_future();
            }
            return p.infoRequests.push(std::make_shared<Private::InfoRequest>());
        }

        std::future<AudioData> AudioRead::readAudio(
            const OTIO_NS::TimeRange& timeRange,
            const IOOptions& options)
        {
            FTK_P();
            auto request = std::make_shared<Private::AudioRequest>();
            request->timeRange = timeRange;
            request->options = merge(options, _options);
            return p.audioRequests.push(request);
        }

        void AudioRead::cancelRequests()
        {
            FTK_P();
            p.infoRequests.cancel();
            p.audioRequests.cancel();
        }

        std::string AudioRead::getError() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.error;
        }

        size_t AudioRead::getErrorCount() const
        {
            FTK_P();
            std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
            return p.errorMutex.count;
        }

        void AudioRead::_run()
        {
            FTK_P();
            // As with the video above: fixed by the probe, and unused when
            // the file has no audio.
            const OTIO_NS::TimeRange audioTime =
                p.info.audioTime.value_or(OTIO_NS::TimeRange());
            p.currentTime = audioTime.start_time();
            p.readAudio->start();
            const bool audioValid = p.info.audio.isValid();
            size_t errorCount = 0;
            while (p.condition.wait())
            {
                // Information requests.
                for (const auto& request : p.infoRequests.popAll())
                {
                    request->promise.set_value(p.info);
                }

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
                        if (!request->timeRange.start_time().strictly_equal(p.currentTime))
                        {
                            seek = true;
                            p.currentTime = request->timeRange.start_time();
                        }
                    }

                    // Seek.
                    if (seek)
                    {
                        p.readAudio->seek(p.currentTime);
                    }

                    // Process.
                    bool intersects = false;
                    if (audioValid)
                    {
                        intersects = request->timeRange.intersects(audioTime);
                    }
                    while (
                        intersects &&
                        p.readAudio->getBufferSize() < request->timeRange.duration().rescaled_to(p.info.audio.sampleRate).value() &&
                        p.readAudio->isValid() &&
                        p.readAudio->process(
                            p.currentTime,
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
                            if (audioData.time < audioTime.start_time())
                            {
                                offset = std::min(
                                    static_cast<size_t>(
                                        (audioTime.start_time() - audioData.time).
                                            rescaled_to(p.info.audio.sampleRate).value()),
                                    requestSampleCount);
                            }
                            p.readAudio->bufferCopy(
                                audioData.audio->getData() + offset * p.info.audio.getByteCount(),
                                audioData.audio->getSampleCount() - offset);
                        }
                    }
                    guard.setValue(std::move(audioData));

                    p.currentTime += request->timeRange.duration();
                }

                // Record any new errors from the worker, logging the
                // first one.
                if (p.readAudio->getErrorCount() != errorCount)
                {
                    const bool first = 0 == errorCount;
                    errorCount = p.readAudio->getErrorCount();
                    {
                        std::unique_lock<std::mutex> lock(p.errorMutex.mutex);
                        p.errorMutex.count = errorCount;
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
                                "tl::ffmpeg::AudioRead",
                                ftk::Format("Errors reading audio: \"{0}\": {1}").
                                    arg(_path.get()).
                                    arg(p.readAudio->getErrorString()),
                                ftk::LogType::Error);
                        }
                    }
                }
            }
        }
    }
}
