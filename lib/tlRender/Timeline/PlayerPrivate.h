// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <tlRender/Timeline/Util.h>

#include <tlRender/Core/AudioResample.h>

#if defined(FTK_SDL2)
#include <SDL2/SDL.h>
#endif // FTK_SDL2
#if defined(FTK_SDL3)
#include <SDL3/SDL.h>
#endif // FTK_SDL3

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

namespace tl
{
    struct Player::Private
    {
        OTIO_NS::RationalTime loopPlayback(const OTIO_NS::RationalTime&, bool& looped);

        void clearRequests();
        void clearCache();
        size_t getVideoCacheMax() const;
        size_t getAudioCacheMax() const;
        OTIO_NS::TimeRange getVideoCacheRange(size_t max) const;
        ftk::Range<int64_t> getAudioCacheRange(size_t max) const;
        void cacheUpdate();
        void cacheEvictAndFill();

        bool hasVideo() const;
        bool hasAudio() const;
        void playbackReset(const OTIO_NS::RationalTime&);
        void resetPlaybackTime(const OTIO_NS::RationalTime&);
        void droppedFramesTick(const OTIO_NS::RationalTime& presentedTime, Playback, double timelineSpeed);
        void audioInit(const std::shared_ptr<ftk::Context>&);
        void audioReset(const OTIO_NS::RationalTime&);
#if defined(FTK_SDL2) || defined(FTK_SDL3)
        void sdlCallback(uint8_t* stream, int len);
#if defined(FTK_SDL2)
        static void sdl2Callback(void* user, Uint8* stream, int len);
#elif defined(FTK_SDL3)
        static void sdl3Callback(void* user, SDL_AudioStream *stream, int additional_amount, int total_amount);
#endif // FTK_SDL2
#endif // FTK_SDL2 || FTK_SDL3

        void log();

        std::weak_ptr<ftk::LogSystem> logSystem;
        PlayerOptions playerOptions;
        std::shared_ptr<Timeline> timeline;
        OTIO_NS::TimeRange timeRange = invalidTimeRange;

        // The audio format of the media, found when the timeline was read. This
        // is stable across media reference keys, because the timeline hands the
        // readers its own audio format to convert to, so every reference is read
        // in the format found when the timeline was read. This is what the audio
        // thread reads, and is not the audio device format; that is audioInfo.
        AudioInfo sourceAudioInfo;

        std::shared_ptr<ftk::Observable<double> > speed;
        std::shared_ptr<ftk::Observable<double> > speedMult;
        std::shared_ptr<ftk::Observable<double> > actualSpeed;
        std::shared_ptr<ftk::Observable<Playback> > playback;
        std::shared_ptr<ftk::Observable<Loop> > loop;
        std::shared_ptr<ftk::Observable<OTIO_NS::RationalTime> > currentTime;
        std::shared_ptr<ftk::Observable<OTIO_NS::RationalTime> > seek;
        std::shared_ptr<ftk::Observable<OTIO_NS::TimeRange> > inOutRange;
        std::shared_ptr<ftk::ObservableList<std::shared_ptr<Timeline> > > compare;
        std::shared_ptr<ftk::Observable<CompareTime> > compareTime;
        std::shared_ptr<ftk::Observable<IOOptions> > ioOptions;
        std::shared_ptr<ftk::Observable<std::string> > mediaReferenceKey;
        std::shared_ptr<ftk::Observable<int> > videoLayer;
        std::shared_ptr<ftk::ObservableList<int> > compareVideoLayers;
        std::shared_ptr<ftk::ObservableList<VideoFrame> > currentVideoFrame;
        std::shared_ptr<ftk::Observable<AudioDeviceID> > audioDevice;
        std::shared_ptr<ftk::Observable<float> > volume;
        std::shared_ptr<ftk::Observable<bool> > mute;
        std::shared_ptr<ftk::ObservableList<bool> > channelMute;
        std::shared_ptr<ftk::Observable<double> > audioOffset;
        std::shared_ptr<ftk::ObservableList<AudioFrame> > currentAudioFrame;
        std::shared_ptr<ftk::Observable<PlayerCacheOptions> > cacheOptions;
        std::shared_ptr<ftk::Observable<PlayerCacheInfo> > cacheInfo;
        std::shared_ptr<ftk::Observable<size_t> > droppedFrames;
        std::shared_ptr<ftk::ListObserver<AudioDeviceInfo> > audioDevicesObserver;
        std::shared_ptr<ftk::Observer<AudioDeviceInfo> > defaultAudioDeviceObserver;

        int accelerate = 0;
        tl::Playback toggle = tl::Playback::Forward;

        // Dropped-frame detection state, owned by the main thread (_tick). The
        // count is how many frames the playback clock passed that were never
        // presented (decode/IO could not keep up): the clock's advance since the
        // baseline, minus the number of distinct frames actually shown. Measuring
        // against the clock (not just frame-to-frame) means a stall that presents
        // no frames at all still accumulates, because the clock keeps moving.
        // droppedFramesReset re-establishes the baseline after a genuine position
        // discontinuity (seek, loop wrap, play start); the stall re-sync
        // deliberately does not raise it. droppedBase carries the running total
        // across baselines so loops keep accumulating; droppedPeak is the
        // high-water deficit since the baseline, so the count never decreases.
        std::optional<OTIO_NS::RationalTime> droppedClockBaseline;
        OTIO_NS::RationalTime droppedPresentedLast = invalidTime;
        int64_t droppedShownCount = 0;
        int64_t droppedPeak = 0;
        size_t droppedBase = 0;
        std::atomic<bool> droppedFramesReset{ true };

        bool audioDevices = false;
        AudioInfo audioInfo;
#if defined(FTK_SDL2)
        int sdlID = 0;
#elif defined(FTK_SDL3)
        SDL_AudioStream* sdlStream = nullptr;
#endif // FTK_SDL2

        std::atomic<bool> running;

        // A snapshot of the playback parameters the main thread publishes to
        // the cache thread. Copied wholesale into Mutex::state under the lock,
        // then lifted into Thread::state (the cache thread's working copy).
        struct PlaybackState
        {
            Playback playback = Playback::Stop;
            OTIO_NS::RationalTime currentTime = invalidTime;
            OTIO_NS::TimeRange inOutRange = invalidTimeRange;
            std::vector<std::shared_ptr<Timeline> > compare;
            CompareTime compareTime = CompareTime::Relative;
            IOOptions ioOptions;
            int videoLayer = 0;
            std::vector<int> compareVideoLayers;
            double audioOffset = 0.0;
            PlayerCacheOptions cacheOptions;

            bool operator == (const PlaybackState&) const;
            bool operator != (const PlaybackState&) const;
        };

        // What the cache eviction and fill depend on. They are idempotent:
        // the same inputs over the same cache do the same work and reach the
        // same result, so when this is unchanged there is nothing to redo.
        // The sizes are what make a completed request, a filled frame or a
        // cleared cache count as a change.
        struct CacheKey
        {
            PlaybackState state;
            CacheDir cacheDir = CacheDir::Forward;
            size_t videoCacheSize = 0;
            size_t audioCacheSize = 0;
            size_t videoRequestsSize = 0;
            size_t audioRequestsSize = 0;
        };

        // Shared between the main thread and the cache thread; every field is
        // guarded by mutex. Main thread -> cache thread: state, the clear*
        // flags, and cacheDir (written by the setters and _tick). Cache thread
        // -> main thread: currentVideoFrame, currentAudioFrame, cacheInfo,
        // which _tick reads back and publishes to the observers.
        struct Mutex
        {
            PlaybackState state;
            bool clearRequests = false;
            bool clearCache = false;
            CacheDir cacheDir = CacheDir::Forward;
            std::vector<VideoFrame> currentVideoFrame;
            std::vector<AudioFrame> currentAudioFrame;
            PlayerCacheInfo cacheInfo;
            std::mutex mutex;
        };
        Mutex mutex;

        // Owned by the cache thread (_thread); no locking. state is its working
        // copy of the last PlaybackState it observed through Mutex; the request
        // and cache maps are its in-flight IO. thread is the handle: started in
        // the constructor, joined by the main thread in the destructor.
        struct Thread
        {
            PlaybackState state;
            CacheDir cacheDir = CacheDir::Forward;
            CacheKey cacheKey;
            bool cacheKeyValid = false;
            std::map<OTIO_NS::RationalTime, std::vector<VideoRequest> > videoRequests;
            std::map<OTIO_NS::RationalTime, std::vector<VideoFrame> > videoCache;
            std::map<int64_t, AudioRequest> audioRequests;
            std::chrono::steady_clock::time_point cacheTimer;
            std::chrono::steady_clock::time_point logTimer;
            std::thread thread;
        };
        Thread thread;

        // The audio parameters the main thread publishes to the audio callback
        // thread; copied wholesale into AudioMutex::state under the lock.
        struct AudioState
        {
            Playback playback = Playback::Stop;
            double speed = 0.0;
            float volume = 1.F;
            bool mute = false;
            std::vector<bool> channelMute;
            std::chrono::steady_clock::time_point muteTimeout;
            double audioOffset = 0.0;
        };

        // Shared by three threads, all guarded by mutex: the main thread, the
        // cache thread, and the audio callback thread. Main thread writes state
        // (the setters) and, via audioReset, reset/start. Cache thread fills and
        // evicts cache and can also reset (the stall re-sync). Audio callback
        // reads state/cache/start, consumes reset, and writes frame back, which
        // the main thread reads in _tick to drive the clock.
        struct AudioMutex
        {
            AudioState state;
            std::map<int64_t, AudioFrame> cache;
            bool reset = false;
            OTIO_NS::RationalTime start = invalidTime;
            int64_t frame = 0;
            std::mutex mutex;
        };
        AudioMutex audioMutex;

        // Owned by the audio callback thread; no locking. The resampler, output
        // buffer, and sample counters that only the callback touches.
        struct AudioThread
        {
            AudioInfo info;
            int64_t inputFrame = 0;
            int64_t outputFrame = 0;
            std::shared_ptr<AudioResample> resample;
            std::list<std::shared_ptr<Audio> > buffer;
            std::shared_ptr<Audio> silence;
        };
        AudioThread audioThread;

        // The wall clock used for timing when no audio device is open: read by
        // the main thread in _tick, written by playbackReset(). Guarded by
        // mutex because the cache thread also resets it from the stall re-sync
        // path in _thread (the no-audio counterpart of audioReset, which guards
        // the audio clock with audioMutex). Writes are rare, so the per-tick
        // read lock is effectively uncontended.
        struct NoAudio
        {
            std::chrono::steady_clock::time_point playbackTimer;
            OTIO_NS::RationalTime start = invalidTime;
            std::mutex mutex;
        };
        NoAudio noAudio;
    };
}
