// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
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
#include <chrono>
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
        ftk::Range<int64_t> getAudioSecondsRange() const;
        ftk::Range<int64_t> getAudioCacheRange(size_t max) const;
        void cacheUpdate();
        void cacheEvictAndFill();

        bool hasVideo() const;
        bool hasAudio() const;
        void playbackReset(const OTIO_NS::RationalTime&);
        void resetPlaybackTime(const OTIO_NS::RationalTime&);
        void droppedFramesTick(
            const std::optional<OTIO_NS::RationalTime>& presentedTime,
            Playback,
            double timelineSpeed);
        void audioInit(const std::shared_ptr<ftk::Context>&);
        int64_t toAudioSamples(const OTIO_NS::RationalTime&) const;
        void audioReset(const OTIO_NS::RationalTime&);
        //! Copy audio from the cache into audioThread.layers, and return how
        //! many samples, or -1 when the cache does not have it.
        int64_t audioRead(const AudioInfo&, Playback, int64_t frame, int64_t size);
        void declick(uint8_t*, size_t sampleCount, const AudioInfo&, bool audio);
#if defined(FTK_SDL2) || defined(FTK_SDL3)
        void sdlCallback(uint8_t* stream, int len);
#if defined(FTK_SDL2)
        static void sdl2Callback(void* user, Uint8* stream, int len);
#elif defined(FTK_SDL3)
        static void sdl3Callback(void* user, SDL_AudioStream *stream, int additional_amount, int total_amount);
#endif // FTK_SDL2
#endif // FTK_SDL2 || FTK_SDL3

        void log();

        //! The media reference key for the comparison timeline at the index.
        std::string compareMediaReferenceKey(size_t) const;

        std::weak_ptr<ftk::LogSystem> logSystem;
        //! What the log calls this player; see Timeline::Private::logId.
        size_t logId = 0;
        PlayerOptions playerOptions;
        std::shared_ptr<Timeline> timeline;
        OTIO_NS::TimeRange timeRange;

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
        std::shared_ptr<ftk::ObservableList<std::string> > compareMediaReferenceKeys;
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
        std::optional<OTIO_NS::RationalTime> droppedPresentedLast;
        int64_t droppedShownCount = 0;
        int64_t droppedPeak = 0;
        size_t droppedBase = 0;
        std::atomic<bool> droppedFramesReset{ true };

        // The wrap count the main thread last saw from the audio callback;
        // a change in it is what a loop looks like from here.
        int64_t audioLoopCount = 0;

        bool audioDevices = false;
        AudioInfo audioInfo;
#if defined(FTK_SDL2)
        int sdlID = 0;
#elif defined(FTK_SDL3)
        SDL_AudioStream* sdlStream = nullptr;
#endif // FTK_SDL2
#if defined(FTK_SDL2) || defined(FTK_SDL3)
        // A reference to SDL's audio, held for as long as the player: see
        // _init().
        bool sdlAudio = false;
#endif // FTK_SDL2 || FTK_SDL3

        std::atomic<bool> running;

        // A snapshot of the playback parameters the main thread publishes to
        // the cache thread. Copied wholesale into Mutex::state under the lock,
        // then lifted into Thread::state (the cache thread's working copy).
        struct PlaybackState
        {
            Playback playback = Playback::Stop;
            OTIO_NS::RationalTime currentTime;
            OTIO_NS::TimeRange inOutRange;
            std::vector<std::shared_ptr<Timeline> > compare;
            CompareTime compareTime = CompareTime::Relative;
            IOOptions ioOptions;
            int videoLayer = 0;
            std::vector<int> compareVideoLayers;
            double audioOffset = 0.0;
            PlayerCacheOptions cacheOptions;

            TL_TIMELINE_API bool operator == (const PlaybackState&) const;
            TL_TIMELINE_API bool operator != (const PlaybackState&) const;
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
            Loop loop = Loop::Loop;
            OTIO_NS::TimeRange inOutRange;
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

            // The playback clock: the callback's read position in source
            // samples, and how many times it has wrapped. While looping the
            // callback keeps the position inside the in/out range, so the
            // main thread reads a position instead of correcting one.
            int64_t position = 0;
            int64_t loops = 0;

            // The clock only moves when the callback runs, which is once
            // per block of audio the device takes: tens of milliseconds on
            // some systems, more than a frame. So the callback also says
            // when it ran, how long its block lasts, and how fast the
            // position moves, and the main thread moves the clock on in
            // between. The generation counts the resets, so a jump that
            // was asked for is not mistaken for jitter.
            std::chrono::steady_clock::time_point positionTime;
            double positionDuration = 0.0;
            double positionRate = 0.0;
            int64_t generation = 0;

            std::mutex mutex;
        };
        AudioMutex audioMutex;

        // The last clock read from the audio, owned by the main thread. The
        // estimate between callbacks can run ahead of where the next one
        // lands, when the blocks vary in size, and the clock holds rather
        // than stepping back a frame.
        struct AudioClock
        {
            int64_t position = 0;
            int64_t loops = 0;
            int64_t generation = -1;
        };
        AudioClock audioClock;

        // Owned by the audio callback thread; no locking. The resampler, output
        // buffer, and sample counters that only the callback touches.
        //
        // Nothing here is allocated by the callback once it is running. A
        // thread that allocates or frees can page fault, and a page fault
        // waits for the process's memory map, which the video threads hold
        // while they map and unmap frames tens of megabytes at a time: the
        // callback stalled for up to half a second, and the device ran dry.
        // So the buffers only grow, when a larger block than any before is
        // asked for, and the samples are copied out of the cache rather
        // than holding on to it, or letting go of the last hold on a second
        // of audio would free it here.
        struct AudioThread
        {
            AudioInfo info;
            int64_t position = 0;
            std::shared_ptr<AudioResample> resample;

            // A copy of AudioMutex::state, kept so that copying into it
            // reuses its storage.
            AudioState state;

            // The source audio for one read: a buffer for each layer, then
            // the layers mixed, then reversed when playing backwards.
            std::vector<std::vector<uint8_t> > layers;
            std::vector<const uint8_t*> layerData;
            std::vector<float> channelVolumes;
            std::vector<uint8_t> mix;
            std::vector<uint8_t> reverse;

            // Resampled audio waiting for the device, in its format.
            std::vector<uint8_t> fifo;
            size_t fifoByteCount = 0;

            // The SDL 3 stream is given audio from here.
            std::vector<uint8_t> sdlBuffer;

            // Declicking. A seek moves the read position from one place in
            // the waveform to another, and a stop or an empty buffer drops
            // it to silence; either is a step between one sample and the
            // next, which is heard as a click. The step is taken from the
            // last frame handed to the device, and an offset starting at it
            // ramps down to nothing over a few milliseconds.
            std::vector<double> lastFrame;
            std::vector<double> declickStep;
            size_t declickPos = 0;
            size_t declickLength = 0;
            bool declickPending = false;
            bool silent = true;
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
            OTIO_NS::RationalTime start;
            std::mutex mutex;
        };
        NoAudio noAudio;
    };
}
