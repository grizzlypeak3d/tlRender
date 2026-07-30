// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/LRUCache.h>

#include <opentimelineio/clip.h>

#include <atomic>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace tl
{
    class ZipReader;

    struct Timeline::Private
    {
        std::weak_ptr<ftk::Context> context;
        std::weak_ptr<ftk::LogSystem> logSystem;
        std::shared_ptr<ftk::FileIO> fileIO;
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline;
        // OTIO works out an item's range in its track by summing the duration
        // of every preceding sibling, and _requests() asks each track child
        // for its range on every request to find the one covering the
        // requested time. Left to OTIO that is quadratic in the number of
        // clips: 20,000 clips took nine seconds to reach the first frame and
        // 100,000 never got there. Filled in once by _init(), which can be
        // done in a single pass per track; the OTIO timeline is never
        // written, so this can be read without locking.
        std::map<const OTIO_NS::Composable*, OTIO_NS::TimeRange> trimmedRangeInParent;
        // The bundle stays open so that a media reference's byte ranges can
        // be worked out when it is first read. Doing it for every reference
        // at open meant generating a file name, decoding it as a URL and
        // parsing it as a path for all 25,000 frames of a bundle before
        // anything could be shown.
        std::shared_ptr<ZipReader> zipReader;
        std::set<const OTIO_NS::MediaReference*> bundleMediaReferences;
        // Always the inner of the two locks: creating a reader holds
        // readCacheMutex and then asks getMem()/mediaUnavailable() where the
        // media lives. Nothing guarded here may reach back for readCacheMutex.
        std::mutex memFilesMutex;
        std::map<const OTIO_NS::MediaReference*,
            std::shared_ptr<std::vector<ftk::MemFile> > > memFiles;
        // Media references named by a bundle but not found inside it. They are
        // not read from their path, since a bundle is meant to be self
        // contained and quietly reading a file from somewhere else would be
        // misleading; reading one of these fails instead. Filled in while the
        // timeline is read and only read afterwards.
        std::set<const OTIO_NS::MediaReference*> unavailableMediaReferences;
        // Guarded by memFilesMutex once the timeline is running, since a
        // reference can also turn out to be unavailable when its byte ranges
        // are worked out on first read.
        bool mediaUnavailable(const OTIO_NS::MediaReference*);
        // Where a media reference's files live inside the bundle, worked out
        // on first use. Shared rather than copied: inside a bundle a
        // sequence reference carries a byte range per frame, and a long one
        // is not a vector to hand out by value.
        std::shared_ptr<std::vector<ftk::MemFile> > getMem(
            const OTIO_NS::MediaReference*);

        // Look up the reader or decoder for a media reference, creating one
        // on a miss. The three caches differ only in what they hold and how
        // an entry is made; the availability checks either side of resolving
        // the byte ranges, the key and the lock are the same for all of
        // them, and were easy to get subtly wrong three times over.
        template<typename T>
        std::shared_ptr<T> getCached(
            ftk::LRUCache<std::string, std::shared_ptr<T> >&,
            const OTIO_NS::MediaReference*,
            const IOOptions&,
            const std::function<std::shared_ptr<T>(
                const std::shared_ptr<ftk::Context>&,
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&)>&);
        ftk::Path path;
        ftk::Path audioPath;
        Options options;
        // Held while a caller drives an unthreaded timeline. _requests()
        // mutates the thread-owned lists without locking, on the assumption
        // that one thread runs it; without a thread that is whichever caller
        // is in getVideo()/getAudio(), and the thumbnail system has three.
        std::mutex driverMutex;
        // Guards the three caches below. They were owned by the request
        // thread, but a timeline opened without one is read by whichever
        // thread drives it, and the thumbnail system drives one from three.
        //
        // Always the outer of the two locks; see memFilesMutex.
        std::mutex readCacheMutex;
        // Video and audio are read by separate readers, cached separately so
        // that a reference read for only one of them -- a silent plate, a
        // bundle's .wav -- costs only that one.
        ftk::LRUCache<std::string, std::shared_ptr<IVideoRead> > videoReadCache;
        ftk::LRUCache<std::string, std::shared_ptr<IAudioRead> > audioReadCache;
        // Sequences, which unlike the read caches hold no thread and no
        // queue: a decoder is stateless, so what is cached here is only where
        // each frame lives. Evicting one costs nothing, which is why this
        // holds far more entries than the read caches can afford to.
        ftk::LRUCache<std::string, std::shared_ptr<SeqDecode> > seqCache;
        // Errors observed while building frames (broken promises caught
        // in videoFrame()/audioFrame()). Owned by the request thread.
        size_t frameErrorCount = 0;
        std::string frameError;
        // High water mark of the reader error total, so the count stays
        // monotonic when readers are evicted from the cache. Owned by
        // the request thread.
        size_t readErrorMax = 0;
        // Media by resolved path, built once while the timeline is read.
        // Resolving a path means decoding a URL and parsing it, so doing it
        // per lookup made every thumbnail request walk the whole timeline.
        std::map<std::string, OTIO_NS::MediaReference*> mediaByPath;
        OTIO_NS::TimeRange timeRange = invalidTimeRange;
        IOInfo ioInfo;
        // The clip whose media references provide the video information, and
        // the information for each of those references. Both are filled in
        // while the timeline is read and only read afterwards, so that
        // getIOInfo() can follow the media reference key without any I/O, and
        // without touching the read cache from the main thread.
        const OTIO_NS::Clip* videoInfoClip = nullptr;
        std::map<const OTIO_NS::MediaReference*, IOInfo> videoInfoByReference;
        // The pixels per unit for OTIO spatial coordinates, taken from the
        // first clip that has them. The coordinates are unit-less, so a
        // reference is needed to map them onto a pixel size. Stays 1.0 when no
        // clip has bounds, where it is unused.
        double boundsScale = 1.0;
        // The canvas shared by the whole timeline, the union of every clip's
        // spatial coordinates. Empty when no clip has bounds, which leaves
        // the layout to the image sizes as before. The offset translates the
        // canvas minimum to the origin.
        ftk::Size2I canvasSize;
        ftk::V2F canvasOffset;
        // The reference size used by Spatial::Normalize for clips that have
        // no spatial coordinates of their own, taken from the first video
        // clip.
        ftk::Size2I normalizeSize;
        // The largest resolution among the media references of the first video
        // clip. The canvas is built from this rather than from the resolution
        // of whichever reference happens to be active when the timeline is
        // read, so that switching to a higher resolution reference is not
        // capped by a canvas built for a proxy. Equal to the resolution of the
        // active reference when a clip has only one.
        ftk::Size2I maxVideoSize;
        uint64_t requestId = 0;

        struct VideoLayerData
        {
            VideoLayerData() {};
            VideoLayerData(VideoLayerData&&) = default;

            std::future<VideoData> image;
            std::future<VideoData> imageB;
            std::optional<ftk::Box2F> bounds;
            std::optional<ftk::Box2F> boundsB;
            Transition transition = Transition::None;
            float transitionValue = 0.F;
        };
        // The internal, in-flight record for a request: the promise the caller
        // is waiting on plus the per-layer IO futures still being assembled.
        // Distinct from the public tl::VideoRequest (id + future) handed back
        // by getVideo(); this is the worker side of that handle.
        struct PendingVideoRequest
        {
            PendingVideoRequest() {};
            PendingVideoRequest(PendingVideoRequest&&) = default;

            uint64_t id = 0;
            OTIO_NS::RationalTime time = invalidTime;
            IOOptions options;
            std::promise<VideoFrame> promise;

            std::vector<VideoLayerData> layerData;
        };

        struct AudioLayerData
        {
            AudioLayerData() {};
            AudioLayerData(AudioLayerData&&) = default;

            double seconds = -1.0;
            OTIO_NS::TimeRange timeRange;
            std::future<AudioData> audio;
        };
        struct PendingAudioRequest
        {
            PendingAudioRequest() {};
            PendingAudioRequest(PendingAudioRequest&&) = default;

            uint64_t id = 0;
            double seconds = -1.0;
            IOOptions options;
            std::promise<AudioFrame> promise;

            std::vector<AudioLayerData> layerData;
        };

        // Shared between the main thread and the request thread; every field
        // is guarded by mutex. The request queues are filled by the main
        // thread (getVideo/getAudio, cancelRequests) and drained by the
        // request thread (_requests). stopped is set by the request thread at
        // shutdown and read by the main thread to reject late requests.
        struct Mutex
        {
            std::list<std::shared_ptr<PendingVideoRequest> > videoRequests;
            std::list<std::shared_ptr<PendingAudioRequest> > audioRequests;
            bool stopped = false;
            std::string readError;
            size_t readErrorCount = 0;
            // The requested media reference keys, handed to the request
            // thread. The timeline wide key applies to clips that have no
            // entry of their own in clipMediaReferenceKeys. An empty key
            // leaves a clip on the media reference that OTIO has active.
            // The OTIO timeline itself is never written, so that it can be
            // read without locking; see Timeline::setMediaReferenceKey().
            std::string mediaReferenceKey;
            std::map<const OTIO_NS::Clip*, std::string> clipMediaReferenceKeys;
            bool mediaReferenceKeysChanged = false;
            std::mutex mutex;
        };
        Mutex mutex;
        // Owned by the request thread; no locking. The in-progress lists hold
        // requests whose IO futures are outstanding. thread and running are the
        // exceptions: the main thread starts the thread (in _init) and clears
        // running (in ~Timeline) to ask it to stop; running is atomic for that
        // handoff.
        struct Thread
        {
            std::list<std::shared_ptr<PendingVideoRequest> > videoRequestsInProgress;
            std::list<std::shared_ptr<PendingAudioRequest> > audioRequestsInProgress;
            std::condition_variable cv;
            std::thread thread;
            std::atomic<bool> running{ false };
            std::chrono::steady_clock::time_point logTimer;
            // Copies of the media reference keys, refreshed under the mutex
            // when the main thread changes them.
            std::string mediaReferenceKey;
            std::map<const OTIO_NS::Clip*, std::string> clipMediaReferenceKeys;
        };
        Thread thread;

        // Where sequence frames are decoded. One pool serves every clip in
        // the timeline rather than a reader thread per clip: with 198 clips
        // and room for ten readers, a single pass used to create and join a
        // thread nearly two hundred times.
        struct ReadPool
        {
            struct Task
            {
                std::function<VideoData()> f;
                std::promise<VideoData> promise;
            };
            std::vector<std::thread> threads;
            std::list<Task> tasks;
            std::condition_variable cv;
            std::mutex mutex;
            bool stopped = false;
        };
        ReadPool readPool;

        // Start and stop the decoding threads.
        void startReadPool(size_t threadCount);
        void stopReadPool();
        // Decode on the pool. The future carries an empty VideoData if the
        // decode throws, which is what a reader did with a failed frame.
        std::future<VideoData> submitRead(std::function<VideoData()>);

        // Give up on a request that has not resolved, so that a caller
        // waiting on its future is not left waiting forever. The frame comes
        // back empty and the reason goes to the log.
        void abandon(const std::shared_ptr<PendingVideoRequest>&);
        void abandon(const std::shared_ptr<PendingAudioRequest>&);

        // Build a finished frame from a request whose futures are ready.
        // Calling these blocks on the layer futures via get(), so callers
        // must ensure readiness (poll with wait_for, or accept the block at
        // shutdown).
        VideoFrame videoFrame(PendingVideoRequest&);
        AudioFrame audioFrame(PendingAudioRequest&);
        // Resolve which media reference a clip should be read from, using the
        // thread-owned key state. Request thread only; the main thread goes
        // through Timeline::getMediaReference(), which takes the mutex.
        OTIO_NS::MediaReference* mediaReference(const OTIO_NS::Clip*) const;

        //! Get a track child's trimmed range in its parent, from
        //! trimmedRangeInParent. Anything not covered by the cache, such as an
        //! item nested below a track, falls back to asking OTIO.
        std::optional<OTIO_NS::TimeRange> getTrimmedRangeInParent(
            const OTIO_NS::Composable*) const;
        // Aggregate reader and frame errors into the mutex-guarded
        // fields. Called on the request thread before completing a
        // request, so the error state is current by the time a caller's
        // future resolves.
        void updateReadErrors();
        std::shared_ptr<Audio> padAudioToOneSecond(
            const std::shared_ptr<Audio>&,
            double seconds,
            const OTIO_NS::TimeRange&);
    };
}
