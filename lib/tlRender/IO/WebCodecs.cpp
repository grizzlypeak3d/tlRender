// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/WebCodecs.h>

#include <tlRender/IO/RequestQueuePrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace tl
{
    namespace webcodecs
    {
        namespace
        {
            //! The block a reader shares with the decode worker; see the
            //! layout comment in WebCodecsWorker.js, which this must
            //! match. The wasm heap is shared memory, so the worker reads
            //! and writes it directly, and the sequence numbers carry the
            //! handshake through Atomics.
            struct Control
            {
                int32_t requestSeq = 0;
                int32_t command = 0;
                double targetUs = 0.0;
                uint32_t ptr = 0;
                uint32_t cap = 0;
                int32_t responseSeq = 0;
                // 0 is RGBA_U8; 1 is planar 4:2:0, written Y then U
                // then V.
                int32_t deliveredFormat = 0;
                double deliveredTs = -1.0;
                int32_t width = 0;
                int32_t height = 0;
                int32_t frameCount = 0;
                int32_t pad2 = 0;
                double duration = 0.0;
                double frameDur = 0.0;
            };
            static_assert(offsetof(Control, targetUs) == 8, "Control layout");
            static_assert(offsetof(Control, responseSeq) == 24, "Control layout");
            static_assert(offsetof(Control, deliveredFormat) == 28, "Control layout");
            static_assert(offsetof(Control, deliveredTs) == 32, "Control layout");
            static_assert(offsetof(Control, width) == 40, "Control layout");
            static_assert(offsetof(Control, duration) == 56, "Control layout");
            static_assert(offsetof(Control, frameDur) == 64, "Control layout");

            //! What the main thread needs for the one dispatch it still
            //! performs: creating the worker and introducing a reader to
            //! it. Everything after that runs without the main thread.
            struct Registration
            {
                std::string url;
                Control* control = nullptr;
                bool audio = false;
            };
            std::mutex registryMutex;
            std::map<int, Registration> registry;
            int registryHandle = 0;
            //! A timed out request's buffer cannot be freed while the
            //! worker may still write into it; a reader that closes
            //! holding one parks it here for good.
            std::vector<std::shared_ptr<void> > parkedForever;

            EM_JS(void, wcRegister, (int handle, const char* url, void* ctrl, int audio), {
                if (!globalThis.__tlWCWorker)
                {
                    // The shell can name the worker with its build id
                    // for cache busting.
                    globalThis.__tlWCWorker = new Worker(
                        globalThis.__tlWCWorkerUrl || 'WebCodecsWorker.js');
                    globalThis.__tlWCWorker.postMessage(
                        { memory: wasmMemory });
                }
                globalThis.__tlWCWorker.postMessage({
                    handle: handle,
                    url: UTF8ToString(url),
                    ctrl: ctrl,
                    audio: 0 !== audio });
            });

            void registerMain(void* arg)
            {
                const int handle = static_cast<int>(
                    reinterpret_cast<intptr_t>(arg));
                std::string url;
                Control* control = nullptr;
                bool audio = false;
                {
                    std::unique_lock<std::mutex> lock(registryMutex);
                    const auto i = registry.find(handle);
                    if (i != registry.end())
                    {
                        url = i->second.url;
                        control = i->second.control;
                        audio = i->second.audio;
                    }
                }
                if (control)
                {
                    wcRegister(handle, url.c_str(), control, audio ? 1 : 0);
                }
            }

            //! Wait for the response sequence number to reach a value,
            //! or the timeout. Returns whether it arrived.
            bool waitResponse(Control* control, int32_t value, double timeoutMs)
            {
                for (;;)
                {
                    const int32_t current = __atomic_load_n(
                        &control->responseSeq, __ATOMIC_SEQ_CST);
                    if (current >= value)
                    {
                        return true;
                    }
                    if (emscripten_futex_wait(
                        &control->responseSeq,
                        current,
                        timeoutMs) == -ETIMEDOUT)
                    {
                        return __atomic_load_n(
                            &control->responseSeq, __ATOMIC_SEQ_CST) >= value;
                    }
                }
            }
        }

        struct VideoRead::Private
        {
            int handle = 0;
            Control* control = nullptr;

            IOInfo info;
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct VideoRequest
            {
                OTIO_NS::RationalTime time;
                std::promise<VideoData> promise;
            };
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<VideoRequest, VideoData> videoRequests{ condition };

            //! The image of the last request that timed out: the worker
            //! may still write the frame into it, so it lives until the
            //! next timeout -- by which time the worker has long since
            //! seen a newer request and can no longer touch it.
            std::shared_ptr<ftk::Image> parked;

            //! Bumped by cancelRequests(): an in-flight wait whose
            //! generation is stale serves a request nobody wants
            //! anymore, and yields to the fresh one.
            std::atomic<int> cancelGen{ 0 };

            std::thread thread;

            struct ErrorMutex
            {
                std::string error;
                size_t count = 0;
                std::mutex mutex;
            };
            ErrorMutex errorMutex;
        };

        void VideoRead::_init(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, {}, options, logSystem);
            FTK_P();

            p.control = new Control;
            {
                std::unique_lock<std::mutex> lock(registryMutex);
                p.handle = ++registryHandle;
                // The page fetches the path as a URL.
                registry[p.handle] = { path.get(), p.control, false };
            }

            p.thread = std::thread(
                [this]
                {
                    FTK_P();
                    try
                    {
                        emscripten_proxy_async(
                            emscripten_proxy_get_system_queue(),
                            emscripten_main_runtime_thread_id(),
                            registerMain,
                            reinterpret_cast<void*>(
                                static_cast<intptr_t>(p.handle)));
                        // The open covers the fetch and the demux, so it
                        // gets a generous timeout.
                        if (!waitResponse(p.control, 1, 30000.0) ||
                            p.control->width <= 0 ||
                            p.control->frameCount <= 0)
                        {
                            throw std::runtime_error(
                                "Cannot open: " + _path.get());
                        }
                        const double rate =
                            p.control->duration > 0.0 ?
                            p.control->frameCount /
                                (p.control->duration / 1000000.0) :
                            24.0;
                        p.info.video.push_back(ftk::ImageInfo(
                            p.control->width,
                            p.control->height,
                            ftk::ImageType::RGBA_U8));
                        p.info.videoTime = OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, rate),
                            OTIO_NS::RationalTime(
                                p.control->frameCount, rate));

                        _run();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::webcodecs::VideoRead",
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

                    p.condition.stopQueues();
                });
        }

        VideoRead::VideoRead() :
            _p(new Private)
        {}

        VideoRead::~VideoRead()
        {
            FTK_P();
            p.condition.stop();
            if (p.thread.joinable())
            {
                p.thread.join();
            }
            p.control->command = 2;
            __atomic_add_fetch(&p.control->requestSeq, 1, __ATOMIC_SEQ_CST);
            emscripten_futex_wake(&p.control->requestSeq, 1);
            {
                std::unique_lock<std::mutex> lock(registryMutex);
                if (p.parked)
                {
                    parkedForever.push_back(p.parked);
                }
                registry.erase(p.handle);
            }
            // The block leaks by design: the worker may still read it
            // after the close lands, and it is 80 bytes per file opened.
        }

        std::shared_ptr<VideoRead> VideoRead::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<VideoRead>(new VideoRead);
            out->_init(path, options, logSystem);
            return out;
        }

        std::future<IOInfo> VideoRead::getInfo()
        {
            FTK_P();
            return p.infoRequests.push(
                std::make_shared<Private::InfoRequest>());
        }

        std::future<VideoData> VideoRead::readVideo(
            const OTIO_NS::RationalTime& time,
            const IOOptions&)
        {
            FTK_P();
            auto request = std::make_shared<Private::VideoRequest>();
            request->time = time;
            return p.videoRequests.push(request);
        }

        void VideoRead::cancelRequests()
        {
            FTK_P();
            ++p.cancelGen;
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
            int32_t seq = 1;
            while (p.condition.wait())
            {
                for (const auto& request : p.infoRequests.popAll())
                {
                    request->promise.set_value(p.info);
                }

                if (auto videoRequest = p.videoRequests.pop())
                {
                    PromiseGuard<VideoData> guard(videoRequest->promise);

                    auto image = ftk::Image::create(p.info.video[0]);
                    // Half a frame of margin so that a frame's own exact
                    // time cannot round to just before it.
                    const double tUs =
                        videoRequest->time.value() /
                            videoRequest->time.rate() * 1000000.0 +
                        p.control->frameDur / 2.0;
                    p.control->targetUs = tUs;
                    p.control->ptr = reinterpret_cast<uintptr_t>(
                        image->getData());
                    p.control->cap = image->getByteCount();
                    p.control->command = 1;
                    ++seq;
                    __atomic_store_n(
                        &p.control->requestSeq, seq, __ATOMIC_SEQ_CST);
                    emscripten_futex_wake(&p.control->requestSeq, 1);

                    // The wait is sliced so that a cancellation -- a
                    // scrub has moved on -- abandons this request within
                    // a slice instead of serializing the drag behind
                    // one-second stalls. Uncancelled, the wait is long
                    // enough for a cold fetch and decode, so waveform
                    // tiles and frames fill instead of coming back
                    // empty.
                    const int gen = p.cancelGen;
                    bool delivered = false;
                    for (int i = 0; i < 150 && !delivered; ++i)
                    {
                        delivered = waitResponse(p.control, seq, 100.0);
                        if (!delivered && gen != p.cancelGen)
                        {
                            break;
                        }
                    }
                    VideoData data;
                    data.time = videoRequest->time;
                    if (delivered && p.control->deliveredTs >= 0.0)
                    {
                        if (1 == p.control->deliveredFormat)
                        {
                            // The worker delivered the decoder's native
                            // planes; the renderer draws planar YUV
                            // itself.
                            auto yuv = ftk::Image::create(ftk::ImageInfo(
                                p.info.video[0].size,
                                ftk::ImageType::YUV_420P_U8));
                            memcpy(
                                yuv->getData(),
                                image->getData(),
                                yuv->getByteCount());
                            data.image = yuv;
                        }
                        else
                        {
                            data.image = image;
                        }
                    }
                    else if (!delivered)
                    {
                        // Freeing this while the worker can still write
                        // into it scribbled over live allocations; the
                        // crashes pointed everywhere but here.
                        p.parked = image;
                    }
                    guard.setValue(std::move(data));
                }
            }
        }


        struct AudioRead::Private
        {
            int handle = 0;
            Control* control = nullptr;

            IOInfo info;
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct AudioRequest
            {
                OTIO_NS::TimeRange timeRange;
                std::promise<AudioData> promise;
            };
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<AudioRequest, AudioData> audioRequests{ condition };

            //! See VideoRead::Private::parked.
            std::shared_ptr<Audio> parked;

            //! See VideoRead::Private::cancelGen.
            std::atomic<int> cancelGen{ 0 };

            std::thread thread;

            struct ErrorMutex
            {
                std::string error;
                size_t count = 0;
                std::mutex mutex;
            };
            ErrorMutex errorMutex;
        };

        void AudioRead::_init(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, {}, options, logSystem);
            FTK_P();

            p.control = new Control;
            {
                std::unique_lock<std::mutex> lock(registryMutex);
                p.handle = ++registryHandle;
                registry[p.handle] = { path.get(), p.control, true };
            }

            p.thread = std::thread(
                [this]
                {
                    FTK_P();
                    try
                    {
                        emscripten_proxy_async(
                            emscripten_proxy_get_system_queue(),
                            emscripten_main_runtime_thread_id(),
                            registerMain,
                            reinterpret_cast<void*>(
                                static_cast<intptr_t>(p.handle)));
                        // The open covers the fetch, the demux, and the
                        // whole track's decode.
                        if (!waitResponse(p.control, 1, 60000.0))
                        {
                            throw std::runtime_error(
                                "Cannot open: " + _path.get());
                        }
                        // No audio track is an answer rather than an
                        // error; the information stays empty.
                        if (p.control->width > 0 &&
                            p.control->height > 0 &&
                            p.control->frameCount > 0)
                        {
                            p.info.audio = AudioInfo(
                                p.control->height,
                                AudioType::F32,
                                p.control->width);
                            p.info.audioTime = OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(
                                    0.0, p.control->width),
                                OTIO_NS::RationalTime(
                                    p.control->frameCount,
                                    p.control->width));
                        }

                        _run();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto logSystem = _logSystem.lock())
                        {
                            logSystem->print(
                                "tl::webcodecs::AudioRead",
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

                    p.condition.stopQueues();
                });
        }

        AudioRead::AudioRead() :
            _p(new Private)
        {}

        AudioRead::~AudioRead()
        {
            FTK_P();
            p.condition.stop();
            if (p.thread.joinable())
            {
                p.thread.join();
            }
            p.control->command = 2;
            __atomic_add_fetch(&p.control->requestSeq, 1, __ATOMIC_SEQ_CST);
            emscripten_futex_wake(&p.control->requestSeq, 1);
            {
                std::unique_lock<std::mutex> lock(registryMutex);
                if (p.parked)
                {
                    parkedForever.push_back(p.parked);
                }
                registry.erase(p.handle);
            }
            // The block leaks by design, like the video reader's.
        }

        std::shared_ptr<AudioRead> AudioRead::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<AudioRead>(new AudioRead);
            out->_init(path, options, logSystem);
            return out;
        }

        std::future<IOInfo> AudioRead::getInfo()
        {
            FTK_P();
            return p.infoRequests.push(
                std::make_shared<Private::InfoRequest>());
        }

        std::future<AudioData> AudioRead::readAudio(
            const OTIO_NS::TimeRange& timeRange,
            const IOOptions&)
        {
            FTK_P();
            auto request = std::make_shared<Private::AudioRequest>();
            request->timeRange = timeRange;
            return p.audioRequests.push(request);
        }

        void AudioRead::cancelRequests()
        {
            FTK_P();
            ++p.cancelGen;
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
            int32_t seq = 1;
            while (p.condition.wait())
            {
                for (const auto& request : p.infoRequests.popAll())
                {
                    request->promise.set_value(p.info);
                }

                if (auto audioRequest = p.audioRequests.pop())
                {
                    PromiseGuard<AudioData> guard(audioRequest->promise);

                    AudioData data;
                    data.time = audioRequest->timeRange.start_time();
                    if (p.info.audio.isValid())
                    {
                        const double rate = p.info.audio.sampleRate;
                        const size_t count = std::max(
                            0.0,
                            audioRequest->timeRange.duration().
                                rescaled_to(rate).value());
                        auto audio = Audio::create(p.info.audio, count);
                        p.control->targetUs =
                            audioRequest->timeRange.start_time().
                                rescaled_to(rate).value();
                        p.control->ptr = reinterpret_cast<uintptr_t>(
                            audio->getData());
                        p.control->cap = audio->getByteCount();
                        p.control->command = 1;
                        ++seq;
                        __atomic_store_n(
                            &p.control->requestSeq, seq, __ATOMIC_SEQ_CST);
                        emscripten_futex_wake(&p.control->requestSeq, 1);

                        // Sliced like the video wait.
                        const int gen = p.cancelGen;
                        bool delivered = false;
                        for (int i = 0; i < 150 && !delivered; ++i)
                        {
                            delivered = waitResponse(p.control, seq, 100.0);
                            if (!delivered && gen != p.cancelGen)
                            {
                                break;
                            }
                        }
                        if (delivered && p.control->deliveredTs >= 0.0)
                        {
                            data.audio = audio;
                        }
                        else if (!delivered)
                        {
                            p.parked = audio;
                        }
                    }
                    guard.setValue(std::move(data));
                }
            }
        }

        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IReadPlugin::_init(
                "WebCodecs",
                {
                    { ".mp4", FileType::Media },
                    { ".m4v", FileType::Media },
                    { ".mov", FileType::Media }
                },
                logSystem);
        }

        ReadPlugin::ReadPlugin()
        {}

        ReadPlugin::~ReadPlugin()
        {}

        std::shared_ptr<ReadPlugin> ReadPlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<ReadPlugin>(new ReadPlugin);
            out->_init(logSystem);
            return out;
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>&,
            const IOOptions& options)
        {
            return VideoRead::create(path, options, _logSystem.lock());
        }

        std::shared_ptr<IAudioRead> ReadPlugin::audioRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>&,
            const IOOptions& options)
        {
            return AudioRead::create(path, options, _logSystem.lock());
        }
    }
}
