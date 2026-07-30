// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/GL/Render.h>

#include <tlRender/Timeline/Timeline.h>

#include <tlRender/IO/System.h>

#include <tlRender/Core/AudioResample.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/Window.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LRUCache.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Timer.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace tl
{
    namespace ui
    {
        bool ThumbnailCacheOptions::operator == (const ThumbnailCacheOptions& other) const
        {
            return
                thumbnailMB == other.thumbnailMB &&
                waveformMB == other.waveformMB;
        }

        bool ThumbnailCacheOptions::operator != (const ThumbnailCacheOptions& other) const
        {
            return !(*this == other);
        }

        namespace
        {
            // Timelines, which hold no thread now that they are opened
            // without one, so a file browser listing can keep the ones it is
            // showing rather than reopening two at a time. Idle entries are
            // dropped after ioCacheTimeout regardless.
            const size_t ioCacheMax   = 100;
            // How long a thread holds its open timelines once it goes idle.
            // The point is to let go of readers, and the decode subprocesses
            // they keep alive, after a file is closed. It has to be long
            // compared to how long opening costs: a bundle of 25,000 entries
            // takes seconds to open, and dropping it between two thumbnails
            // meant most of the time went into opening it again.
            const std::chrono::seconds ioCacheTimeout(60);
            const size_t infoCacheMax = 1000;

            std::string getInfoKey(
                const ftk::Path& path,
                const ftk::Path& mediaPath,
                const IOOptions& options)
            {
                std::stringstream ss;
                ss << path.get() << ";" << mediaPath.get() << ";";
                for (const auto& i : options)
                {
                    ss << i.first << ":" << i.second << ";";
                }
                return ss.str();
            }


            // Every thumbnail is a frame of a timeline. A plain file is a
            // timeline of one clip, so one path covers files, bundles and
            // timelines alike, and nothing needs a syntax for naming what is
            // inside a bundle.
            std::shared_ptr<Timeline> getTimeline(
                const std::shared_ptr<ftk::Context>& context,
                ftk::LRUCache<std::string, std::shared_ptr<Timeline> >& cache,
                std::mutex& mutex,
                const ftk::Path& path)
            {
                // One timeline per file, shared by the three threads rather
                // than one each. Opening a bundle of 25,000 entries takes
                // seconds, so opening it three times is three times too
                // many. The timeline has no thread of its own and guards its
                // caches, so the threads can read it at once.
                std::shared_ptr<Timeline> out;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    if (cache.get(path.get(), out))
                    {
                        return out;
                    }
                }

                // Opened without the lock: a bundle takes seconds and a movie
                // needs a probe, and holding the lock across that would stall
                // every request for every other file. Two threads opening the
                // same file at once is cheaper than that, and only one of the
                // two ends up in the cache.
                Options options;
                options.threaded = false;
                out = Timeline::create(context, path, options);
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    std::shared_ptr<Timeline> other;
                    if (cache.get(path.get(), other))
                    {
                        return other;
                    }
                    cache.add(path.get(), out);
                }
                return out;
            }

            std::string getThumbnailKey(
                const ftk::Path& path,
                const ftk::Path& mediaPath,
                int height,
                const OTIO_NS::RationalTime& time,
                const IOOptions& options)
            {
                std::stringstream ss;
                ss << path.get() << ";" << mediaPath.get() << ";" <<
                    height << ";" << time << ";";
                for (const auto& i : options)
                {
                    ss << i.first << ":" << i.second << ";";
                }
                return ss.str();
            }

            std::string getWaveformKey(
                const ftk::Path& path,
                const ftk::Path& mediaPath,
                const ftk::Size2I& size,
                const OTIO_NS::TimeRange& timeRange,
                const IOOptions& options)
            {
                std::stringstream ss;
                ss << path.get() << ";" << mediaPath.get() << ";" <<
                    size << ";" << timeRange << ";";
                for (const auto& i : options)
                {
                    ss << i.first << ":" << i.second << ";";
                }
                return ss.str();
            }
        }

        struct ThumbnailSystem::Private
        {
            std::weak_ptr<ftk::Context> context;
            std::shared_ptr<ftk::gl::Window> window;
            uint64_t requestId = 0;
            std::shared_ptr<ftk::Observable<ThumbnailCacheOptions> > cacheOptions;

            struct InfoRequest
            {
                uint64_t id = 0;
                ftk::Path path;
                ftk::Path mediaPath;
                IOOptions options;
                std::promise<IOInfo> promise;
            };

            struct ThumbnailRequest
            {
                uint64_t id = 0;
                ftk::Path path;
                ftk::Path mediaPath;
                int height = 0;
                OTIO_NS::RationalTime time = invalidTime;
                IOOptions options;
                std::promise<std::shared_ptr<ftk::Image> > promise;
            };

            struct WaveformRequest
            {
                uint64_t id = 0;
                ftk::Path path;
                ftk::Path mediaPath;
                ftk::Size2I size;
                OTIO_NS::TimeRange timeRange = invalidTimeRange;
                IOOptions options;
                std::promise<std::shared_ptr<ftk::TriMesh2F> > promise;
            };

            struct InfoMutex
            {
                std::list<std::shared_ptr<InfoRequest> > requests;
                ftk::LRUCache<std::string, IOInfo> cache;
                bool stopped = false;
                std::mutex mutex;
            };
            InfoMutex infoMutex;

            struct ThumbnailMutex
            {
                std::list<std::shared_ptr<ThumbnailRequest> > requests;
                ftk::LRUCache<std::string, std::shared_ptr<ftk::Image> > cache;
                bool stopped = false;
                std::mutex mutex;
            };
            ThumbnailMutex thumbnailMutex;

            struct WaveformMutex
            {
                std::list<std::shared_ptr<WaveformRequest> > requests;
                ftk::LRUCache<std::string, std::shared_ptr<ftk::TriMesh2F> > cache;
                bool stopped = false;
                std::mutex mutex;
            };
            WaveformMutex waveformMutex;

            // Shared by the three threads below.
            ftk::LRUCache<std::string, std::shared_ptr<Timeline> > ioCache;
            std::mutex ioCacheMutex;

            struct InfoThread
            {
                std::atomic<bool> ioCacheClear = false;
                std::condition_variable cv;
                std::thread thread;
                std::atomic<bool> running;
            };
            InfoThread infoThread;

            struct ThumbnailThread
            {
                std::shared_ptr<gl::Render> render;
                std::shared_ptr<ftk::gl::OffscreenBuffer> buffer;
                std::atomic<bool> ioCacheClear = false;
                std::condition_variable cv;
                std::thread thread;
                std::atomic<bool> running;
            };
            ThumbnailThread thumbnailThread;

            struct WaveformThread
            {
                std::atomic<bool> ioCacheClear = false;
                std::condition_variable cv;
                std::thread thread;
                std::atomic<bool> running;
            };
            WaveformThread waveformThread;

            std::shared_ptr<ftk::Timer> logTimer;

            // When any of the three threads last had work. The cache is
            // shared, so one thread going quiet must not drop the timelines
            // another is still using.
            std::atomic<int64_t> ioCacheActive{ 0 };
            void ioCacheTouch()
            {
                ioCacheActive = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
            }
            bool ioCacheIdle(const std::chrono::seconds& timeout)
            {
                const auto last = std::chrono::steady_clock::time_point(
                    std::chrono::steady_clock::duration(ioCacheActive.load()));
                return std::chrono::steady_clock::now() - last > timeout;
            }
            void clearIOCache()
            {
                std::unique_lock<std::mutex> lock(ioCacheMutex);
                ioCache.clear();
            }
            size_t ioCacheCount()
            {
                std::unique_lock<std::mutex> lock(ioCacheMutex);
                return ioCache.getCount();
            }
        };

        ThumbnailSystem::ThumbnailSystem(const std::shared_ptr<ftk::Context>& context) :
            ISystem(context, "tl::ui::ThumbnailSystem"),
            _p(new Private)
        {
            FTK_P();
            
            p.context = context;

            p.window = ftk::gl::Window::create(
                context,
                "tl::ui::ThumbnailSystem",
                ftk::Size2I(1, 1),
                static_cast<int>(ftk::gl::WindowOptions::None));

            p.cacheOptions = ftk::Observable<ThumbnailCacheOptions>::create();

            p.infoMutex.cache.setMax(infoCacheMax);
            p.infoThread.running = true;
            p.infoThread.thread = std::thread(
                [this]
                {
                    FTK_P();
                    _infoRun();
                    {
                        std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                        p.infoMutex.stopped = true;
                    }
                    _infoCancel();
                });

            p.thumbnailMutex.cache.setMax(p.cacheOptions->get().thumbnailMB * ftk::megabyte);
            p.ioCache.setMax(ioCacheMax);
            p.thumbnailThread.running = true;
            p.thumbnailThread.thread = std::thread(
                [this]
                {
                    FTK_P();
                    p.window->makeCurrent();
                    if (auto context = p.context.lock())
                    {
                        p.thumbnailThread.render = gl::Render::create(
                            context->getLogSystem(),
                            context->getSystem<ftk::FontSystem>());
                    }
                    if (p.thumbnailThread.render)
                    {
                        _thumbnailRun();
                    }
                    {
                        std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                        p.thumbnailMutex.stopped = true;
                    }
                    p.thumbnailThread.buffer.reset();
                    p.thumbnailThread.render.reset();
                    _thumbnailCancel();
                    p.window->clearCurrent();
                });

            p.waveformMutex.cache.setMax(p.cacheOptions->get().waveformMB * ftk::megabyte);
            
            p.waveformThread.running = true;
            p.waveformThread.thread = std::thread(
                [this]
                {
                    FTK_P();
                    _waveformRun();
                    {
                        std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                        p.waveformMutex.stopped = true;
                    }
                    _waveformCancel();
                });

            p.logTimer = ftk::Timer::create(context);
            p.logTimer->setRepeating(true);
            p.logTimer->start(
                std::chrono::seconds(10),
                [this]
                {
                    FTK_P();
                    if (auto context = p.context.lock())
                    {
                        size_t infoCacheSize = 0;
                        size_t thumbnailCacheSize = 0;
                        size_t waveformCacheSize = 0;
                        {
                            std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                            infoCacheSize = p.infoMutex.cache.getSize();
                        }
                        {
                            std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                            thumbnailCacheSize = p.thumbnailMutex.cache.getSize();
                        }
                        {
                            std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                            waveformCacheSize = p.waveformMutex.cache.getSize();
                        }
                        auto logSystem = context->getLogSystem();
                        logSystem->print(
                            "tl::ui::ThumbnailSystem",
                            ftk::Format(
                                "\n"
                                "    * Information: {0}/{1}\n"
                                "    * Thumbnails: {2}/{3}MB\n"
                                "    * Waveforms: {4}/{5}MB"
                            ).
                            arg(infoCacheSize).
                            arg(infoCacheMax).
                            arg(thumbnailCacheSize / ftk::megabyte).
                            arg(p.cacheOptions->get().thumbnailMB).
                            arg(waveformCacheSize / ftk::megabyte).
                            arg(p.cacheOptions->get().waveformMB));
                    }
                });
        }

        ThumbnailSystem::~ThumbnailSystem()
        {
            FTK_P();
            p.infoThread.running = false;
            p.thumbnailThread.running = false;
            p.waveformThread.running = false;
            if (p.infoThread.thread.joinable())
            {
                p.infoThread.thread.join();
            }
            if (p.thumbnailThread.thread.joinable())
            {
                p.thumbnailThread.thread.join();
            }
            if (p.waveformThread.thread.joinable())
            {
                p.waveformThread.thread.join();
            }
        }

        std::shared_ptr<ThumbnailSystem> ThumbnailSystem::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            auto out = context->getSystem<ThumbnailSystem>();
            if (!out)
            {
                out = std::shared_ptr<ThumbnailSystem>(new ThumbnailSystem(context));
                context->addSystem(out);
            }
            return out;
        }

        InfoRequest ThumbnailSystem::getInfo(
            const ftk::Path& path,
            const IOOptions& options)
        {
            return getInfo(path, path, options);
        }

        InfoRequest ThumbnailSystem::getInfo(
            const ftk::Path& path,
            const ftk::Path& mediaPath,
            const IOOptions& options)
        {
            FTK_P();
            (p.requestId)++;

            auto request = std::make_shared<Private::InfoRequest>();
            request->id = p.requestId;
            request->path = path;
            request->mediaPath = mediaPath;
            request->options = options;

            const std::string key = getInfoKey(path, mediaPath, options);
            IOInfo info;
            bool notify = false;
            {
                std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                if (p.infoMutex.cache.get(key, info))
                    ;
                else if (!p.infoMutex.stopped)
                {
                    notify = true;
                    p.infoMutex.requests.push_back(request);
                }
            }
            if (notify)
            {
                p.infoThread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(info);
            }

            InfoRequest out;
            out.id = request->id;
            out.future = request->promise.get_future();
            return out;
        }

        ThumbnailRequest ThumbnailSystem::getThumbnail(
            const ftk::Path& path,
            int height,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            return getThumbnail(path, path, height, time, options);
        }

        ThumbnailRequest ThumbnailSystem::getThumbnail(
            const ftk::Path& path,
            const ftk::Path& mediaPath,
            int height,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            FTK_P();
            (p.requestId)++;

            auto request = std::make_shared<Private::ThumbnailRequest>();
            request->id = p.requestId;
            request->path = path;
            request->mediaPath = mediaPath;
            request->height = height;
            request->time = time;
            request->options = options;

            const std::string key = getThumbnailKey(
                path,
                mediaPath,
                height,
                time,
                options);
            std::shared_ptr<ftk::Image> thumbnail;
            bool notify = false;
            {
                std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                if (p.thumbnailMutex.cache.get(key, thumbnail))
                    ;
                else if (!p.thumbnailMutex.stopped)
                {
                    notify = true;
                    p.thumbnailMutex.requests.push_back(request);
                }
            }
            if (notify)
            {
                p.thumbnailThread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(thumbnail);
            }

            ThumbnailRequest out;
            out.id = request->id;
            out.height = height;
            out.time = time;
            out.future = request->promise.get_future();
            return out;
        }

        WaveformRequest ThumbnailSystem::getWaveform(
            const ftk::Path& path,
            const ftk::Size2I& size,
            const OTIO_NS::TimeRange& range,
            const IOOptions& options)
        {
            return getWaveform(path, {}, size, range, options);
        }

        WaveformRequest ThumbnailSystem::getWaveform(
            const ftk::Path& path,
            const ftk::Path& mediaPath,
            const ftk::Size2I& size,
            const OTIO_NS::TimeRange& timeRange,
            const IOOptions& options)
        {
            FTK_P();
            (p.requestId)++;

            auto request = std::make_shared<Private::WaveformRequest>();
            request->id = p.requestId;
            request->path = path;
            request->mediaPath = mediaPath;
            request->size = size;
            request->timeRange = timeRange;
            request->options = options;

            const std::string key = getWaveformKey(
                path,
                mediaPath,
                size,
                timeRange,
                options);
            std::shared_ptr<ftk::TriMesh2F> mesh;
            bool notify = false;
            {
                std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                if (p.waveformMutex.cache.get(key, mesh))
                    ;
                else if (!p.waveformMutex.stopped)
                {
                    notify = true;
                    p.waveformMutex.requests.push_back(request);
                }
            }
            if (notify)
            {
                p.waveformThread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(mesh);
            }

            WaveformRequest out;
            out.id = request->id;
            out.size = size;
            out.timeRange = timeRange;
            out.future = request->promise.get_future();
            return out;
        }

        void ThumbnailSystem::cancelRequests(const std::vector<uint64_t>& ids)
        {
            FTK_P();
            // Looked up as a set: this is called with the requests of a whole
            // timeline's worth of items, and searching the list of ids for each
            // pending request made cancelling cost the product of the two.
            const std::set<uint64_t> idSet(ids.begin(), ids.end());
            {
                std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                auto i = p.infoMutex.requests.begin();
                while (i != p.infoMutex.requests.end())
                {
                    if (idSet.find((*i)->id) != idSet.end())
                    {
                        i = p.infoMutex.requests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            {
                std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                auto i = p.thumbnailMutex.requests.begin();
                while (i != p.thumbnailMutex.requests.end())
                {
                    if (idSet.find((*i)->id) != idSet.end())
                    {
                        i = p.thumbnailMutex.requests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            {
                std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                auto i = p.waveformMutex.requests.begin();
                while (i != p.waveformMutex.requests.end())
                {
                    if (idSet.find((*i)->id) != idSet.end())
                    {
                        i = p.waveformMutex.requests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
        }

        const ThumbnailCacheOptions& ThumbnailSystem::getCacheOptions() const
        {
            return _p->cacheOptions->get();
        }

        std::shared_ptr<ftk::IObservable<ThumbnailCacheOptions> > ThumbnailSystem::observeCacheOptions() const
        {
            return _p->cacheOptions;
        }

        void ThumbnailSystem::setCacheOptions(const ThumbnailCacheOptions& value)
        {
            FTK_P();
            if (p.cacheOptions->setIfChanged(value))
            {
                {
                    std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                    p.thumbnailMutex.cache.setMax(value.thumbnailMB * ftk::megabyte);
                }
                {
                    std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                    p.waveformMutex.cache.setMax(value.waveformMB * ftk::megabyte);
                }
            }
        }

        void ThumbnailSystem::clearCache()
        {
            FTK_P();
            {
                std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                p.infoMutex.cache.clear();
            }
            {
                std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                p.thumbnailMutex.cache.clear();
            }
            {
                std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                p.waveformMutex.cache.clear();
            }

            // Signal the worker threads to drop their cached open readers so a
            // reloaded file is re-opened rather than served from a reader that
            // points at the previous file contents. The ioCache is owned by its
            // worker thread, so it is cleared there rather than under a lock.
            p.infoThread.ioCacheClear = true;
            p.infoThread.cv.notify_one();
            p.thumbnailThread.ioCacheClear = true;
            p.thumbnailThread.cv.notify_one();
            p.waveformThread.ioCacheClear = true;
            p.waveformThread.cv.notify_one();
        }

        void ThumbnailSystem::_infoRun()
        {
            FTK_P();
            while (p.infoThread.running)
            {
                if (p.infoThread.ioCacheClear.exchange(false))
                {
                    p.clearIOCache();
                }
                std::shared_ptr<Private::InfoRequest> request;
                {
                    std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                    if (p.infoThread.cv.wait_for(
                        lock,
                        std::chrono::milliseconds(5),
                        [this]
                        {
                            return !_p->infoMutex.requests.empty();
                        }))
                    {
                        request = p.infoMutex.requests.front();
                        p.infoMutex.requests.pop_front();
                    }
                }
                if (request)
                {
                    p.ioCacheTouch();
                    IOInfo info;
                    try
                    {
                        //std::cout << "info request: " << request->path.get() << std::endl;
                        auto context = p.context.lock();
                        if (auto timeline = getTimeline(
                            context, p.ioCache, p.ioCacheMutex, request->path))
                        {
                            timeline->getMediaInfo(request->mediaPath, info);
                        }
                    }
                    catch (const std::exception&)
                    {}
                    request->promise.set_value(info);

                    const std::string key = getInfoKey(
                        request->path, request->mediaPath, request->options);
                    std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                    p.infoMutex.cache.add(key, info);
                }
            }
        }

        void ThumbnailSystem::_thumbnailRun()
        {
            FTK_P();
            tl::IOOptions ioOptions;
            while (p.thumbnailThread.running)
            {
                if (p.thumbnailThread.ioCacheClear.exchange(false))
                {
                    p.clearIOCache();
                }

                std::shared_ptr<Private::ThumbnailRequest> request;
                {
                    std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                    if (p.thumbnailThread.cv.wait_for(
                        lock,
                        std::chrono::milliseconds(5),
                        [this]
                        {
                            return !_p->thumbnailMutex.requests.empty();
                        }))
                    {
                        request = p.thumbnailMutex.requests.front();
                        p.thumbnailMutex.requests.pop_front();
                    }
                }
                if (request)
                {
                    // The options belong to the read, not to the timeline
                    // that is read from: a per clip option such as a camera
                    // name changes with every request, and dropping the
                    // timeline for it meant reopening the file each time.
                    p.ioCacheTouch();

                    std::shared_ptr<ftk::Image> image;
                    try
                    {
                        //std::cout << "thumbnail request: " <<
                        //    request->path.get() << " " <<
                        //    request->time << std::endl;
                        auto context = p.context.lock();
                        auto timeline = getTimeline(
                            context, p.ioCache, p.ioCacheMutex, request->path);
                        IOInfo info;
                        if (timeline &&
                            timeline->getMediaInfo(
                                request->mediaPath, info, request->options))
                        {
                            ftk::Size2I size;
                            if (!info.video.empty())
                            {
                                size.w = request->height * ftk::aspectRatio(info.video[0].size);
                                size.h = request->height;
                            }
                            if (size.isValid())
                            {
                                if (ftk::gl::doCreate(
                                    p.thumbnailThread.buffer,
                                    size,
                                    ftk::gl::TextureType::RGBA_U8))
                                {
                                    p.thumbnailThread.buffer = ftk::gl::OffscreenBuffer::create(
                                        size,
                                        ftk::gl::TextureType::RGBA_U8);
                                }
                                // isValid(), not a comparison against
                                // invalidTime: that constant is -1/-1, which
                                // is one second, and comparing times rescales
                                // them. Every request at one second -- frame
                                // 24 at 24fps -- would read as "no time given"
                                // and quietly return the first frame instead.
                                const OTIO_NS::RationalTime time =
                                    isValid(request->time) ?
                                    request->time :
                                    info.videoTime.start_time();
                                auto videoRequest = timeline->readMedia(
                                    request->mediaPath, time, request->options);
                                if (videoRequest.valid())
                                {
                                    const auto videoData = videoRequest.get();
                                    if (p.thumbnailThread.render &&
                                        p.thumbnailThread.buffer &&
                                        videoData.image &&
                                        p.thumbnailThread.running)
                                    {
                                        ftk::gl::OffscreenBufferBinding binding(p.thumbnailThread.buffer);
                                        p.thumbnailThread.render->begin(size);
                                        ftk::ImageOptions imageOptions;
                                        imageOptions.cache = false;
                                        p.thumbnailThread.render->IRender::drawImage(
                                            videoData.image,
                                            ftk::Box2I(0, 0, size.w, size.h),
                                            ftk::Color4F(1.F, 1.F, 1.F),
                                            imageOptions);
                                        p.thumbnailThread.render->end();
                                        image = ftk::Image::create(
                                            ftk::ImageInfo(size.w, size.h, ftk::ImageType::RGBA_U8));
                                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                                        glReadPixels(
                                            0,
                                            0,
                                            size.w,
                                            size.h,
                                            GL_RGBA,
                                            GL_UNSIGNED_BYTE,
                                            image->getData());
                                    }
                                }
                            }
                        }
                        else if (timeline)
                        {
                            // The request does not name media inside the
                            // timeline, so it wants a picture of the timeline
                            // itself. Use the one already open: creating
                            // another here read the file again for every
                            // request, which on a bundle of 25,000 entries
                            // meant a thumbnail took as long as an open.
                            if (auto logSystem = context->getLogSystem())
                            {
                                logSystem->print("tl::ui::ThumbnailSystem",
                                    ftk::Format("Media not found in timeline, "
                                        "using the timeline itself: \"{0}\" "
                                        "in \"{1}\"").
                                        arg(request->mediaPath.get()).
                                        arg(request->path.get()),
                                    ftk::LogType::Warning);
                            }
                            const auto info = timeline->getIOInfo();
                            const auto videoData = timeline->getVideo(
                                isValid(request->time) ?
                                    request->time :
                                    timeline->getTimeRange().start_time()).future.get();
                            ftk::Size2I size;
                            if (!info.video.empty())
                            {
                                size.w = request->height * ftk::aspectRatio(info.video.front().size);
                                size.h = request->height;
                            }
                            if (size.isValid())
                            {
                                if (ftk::gl::doCreate(
                                    p.thumbnailThread.buffer,
                                    size,
                                    ftk::gl::TextureType::RGBA_U8))
                                {
                                    p.thumbnailThread.buffer = ftk::gl::OffscreenBuffer::create(
                                        size,
                                        ftk::gl::TextureType::RGBA_U8);
                                }
                                if (p.thumbnailThread.render && p.thumbnailThread.buffer)
                                {
                                    ftk::gl::OffscreenBufferBinding binding(p.thumbnailThread.buffer);
                                    p.thumbnailThread.render->begin(size);
                                    p.thumbnailThread.render->drawVideo(
                                        { videoData },
                                        { ftk::Box2I(0, 0, size.w, size.h) });
                                    p.thumbnailThread.render->end();
                                    ftk::ImageInfo info(size.w,
                                        size.h,
                                        ftk::ImageType::RGBA_U8);
                                    image = ftk::Image::create(info);
                                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                                    glReadPixels(
                                        0,
                                        0,
                                        size.w,
                                        size.h,
                                        GL_RGBA,
                                        GL_UNSIGNED_BYTE,
                                        image->getData());
                                }
                            }
                        }
                    }
                    catch (const std::exception&)
                    {}
                    request->promise.set_value(image);

                    const std::string key = getThumbnailKey(
                        request->path,
                        request->mediaPath,
                        request->height,
                        request->time,
                        request->options);
                    std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                    p.thumbnailMutex.cache.add(key, image, image ? image->getByteCount() : 0);
                }
                else if (p.ioCacheCount() > 0 && p.ioCacheIdle(ioCacheTimeout))
                {
                    // Release cached readers (and the decode subprocesses they
                    // keep alive) once this thread has gone idle. Otherwise a
                    // file's readers linger until a different file's readers
                    // push them out of the LRU caches, so closing a file leaves
                    // its ffmpeg process running until the next file is opened.
                    p.clearIOCache();
                }
            }
        }

        namespace
        {
            std::shared_ptr<ftk::TriMesh2F> audioMesh(
                const std::shared_ptr<Audio>& audio,
                const ftk::Size2I& size)
            {
                auto out = std::shared_ptr<ftk::TriMesh2F>(new ftk::TriMesh2F);
                const auto& info = audio->getInfo();
                const size_t sampleCount = audio->getSampleCount();
                if (sampleCount > 0)
                {
                    switch (info.type)
                    {
                    case AudioType::F32:
                    {
                        const float* data = reinterpret_cast<const float*>(
                            audio->getData());
                        for (int x = 0; x < size.w; ++x)
                        {
                            const int x0 = static_cast<int>(std::min(
                                static_cast<size_t>((x + 0) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1));
                            const int x1 = static_cast<int>(std::min(
                                static_cast<size_t>((x + 1) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1));
                            //std::cout << x << ": " << x0 << " " << x1 << std::endl;
                            float min = 0.F;
                            float max = 0.F;
                            if (x0 <= x1)
                            {
                                min = std::numeric_limits<float>::max();
                                max = std::numeric_limits<float>::lowest();
                                for (int i = x0; i <= x1 && i < static_cast<int>(sampleCount); ++i)
                                {
                                    const float v = *(data + i * info.channelCount);
                                    min = std::min(min, v);
                                    max = std::max(max, v);
                                }
                            }
                            const int h2 = size.h / 2;
                            const ftk::Box2I box(
                                ftk::V2I(
                                    x,
                                    h2 - h2 * max),
                                ftk::V2I(
                                    x + 1,
                                    h2 - h2 * min));
                            if (box.isValid())
                            {
                                const size_t j = 1 + out->v.size();
                                out->v.push_back(ftk::V2F(box.x(), box.y()));
                                out->v.push_back(ftk::V2F(box.x() + box.w(), box.y()));
                                out->v.push_back(ftk::V2F(box.x() + box.w(), box.y() + box.h()));
                                out->v.push_back(ftk::V2F(box.x(), box.y() + box.h()));
                                out->triangles.push_back(ftk::Triangle2({ j + 0, j + 2, j + 1 }));
                                out->triangles.push_back(ftk::Triangle2({ j + 2, j + 0, j + 3 }));
                            }
                        }
                        break;
                    }
                    default: break;
                    }
                }
                return out;
            }
        }

        void ThumbnailSystem::_waveformRun()
        {
            FTK_P();
            tl::IOOptions ioOptions;
            while (p.waveformThread.running)
            {
                if (p.waveformThread.ioCacheClear.exchange(false))
                {
                    p.clearIOCache();
                }

                std::shared_ptr<Private::WaveformRequest> request;
                {
                    std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                    if (p.waveformThread.cv.wait_for(
                        lock,
                        std::chrono::milliseconds(5),
                        [this]
                        {
                            return !_p->waveformMutex.requests.empty();
                        }))
                    {
                        request = p.waveformMutex.requests.front();
                        p.waveformMutex.requests.pop_front();
                    }
                }
                if (request)
                {
                    // The options belong to the read, not to the timeline
                    // that is read from: a per clip option such as a camera
                    // name changes with every request, and dropping the
                    // timeline for it meant reopening the file each time.
                    p.ioCacheTouch();

                    std::shared_ptr<ftk::TriMesh2F> mesh;
                    try
                    {
                        auto context = p.context.lock();
                        auto timeline = getTimeline(
                            context, p.ioCache, p.ioCacheMutex, request->path);
                        IOInfo info;
                        if (timeline &&
                            timeline->getMediaInfo(
                                request->mediaPath, info, request->options))
                        {
                            const OTIO_NS::TimeRange timeRange =
                                isValid(request->timeRange) ?
                                request->timeRange :
                                OTIO_NS::TimeRange(
                                    OTIO_NS::RationalTime(0.0, 1.0),
                                    OTIO_NS::RationalTime(1.0, 1.0));
                            auto audioRequest = timeline->readMediaAudio(
                                request->mediaPath, timeRange, request->options);
                            if (audioRequest.valid())
                            {
                                const auto audioData = audioRequest.get();
                                if (audioData.audio && p.waveformThread.running)
                                {
                                    auto resample = AudioResample::create(
                                        audioData.audio->getInfo(),
                                        AudioInfo(1, AudioType::F32, audioData.audio->getSampleRate()));
                                    if (auto resampledAudio = resample->process(audioData.audio))
                                    {
                                        mesh = audioMesh(resampledAudio, request->size);
                                    }
                                }
                            }
                        }
                    }
                    catch (const std::exception&)
                    {}
                    request->promise.set_value(mesh);

                    const std::string key = getWaveformKey(
                        request->path,
                        request->mediaPath,
                        request->size,
                        request->timeRange,
                        request->options);
                    std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                    p.waveformMutex.cache.add(key, mesh, mesh ? mesh->getByteCount() : 0);
                }
                else if (p.ioCacheCount() > 0 && p.ioCacheIdle(ioCacheTimeout))
                {
                    // Release cached readers (and the decode subprocesses they
                    // keep alive) once this thread has gone idle; see the note in
                    // _thumbnailRun().
                    p.clearIOCache();
                }
            }
        }

        void ThumbnailSystem::_infoCancel()
        {
            FTK_P();
            std::list<std::shared_ptr<Private::InfoRequest> > requests;
            {
                std::unique_lock<std::mutex> lock(p.infoMutex.mutex);
                requests = std::move(p.infoMutex.requests);
            }
            for (auto& request : requests)
            {
                request->promise.set_value(IOInfo());
            }
        }

        void ThumbnailSystem::_thumbnailCancel()
        {
            FTK_P();
            std::list<std::shared_ptr<Private::ThumbnailRequest> > requests;
            {
                std::unique_lock<std::mutex> lock(p.thumbnailMutex.mutex);
                requests = std::move(p.thumbnailMutex.requests);
            }
            for (auto& request : requests)
            {
                request->promise.set_value(nullptr);
            }
        }

        void ThumbnailSystem::_waveformCancel()
        {
            FTK_P();
            std::list<std::shared_ptr<Private::WaveformRequest> > requests;
            {
                std::unique_lock<std::mutex> lock(p.waveformMutex.mutex);
                requests = std::move(p.waveformMutex.requests);
            }
            for (auto& request : requests)
            {
                request->promise.set_value(nullptr);
            }
        }

        void to_json(nlohmann::json& json, const ThumbnailCacheOptions& value)
        {
            json["ThumbnailMB"] = value.thumbnailMB;
            json["WaveformMB"] = value.waveformMB;
        }

        void from_json(const nlohmann::json& json, ThumbnailCacheOptions& value)
        {
            json.at("ThumbnailMB").get_to(value.thumbnailMB);
            json.at("WaveformMB").get_to(value.waveformMB);
        }
    }
}
