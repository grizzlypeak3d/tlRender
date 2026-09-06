// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/PlayerPrivate.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <cmath>

namespace tl
{
    OTIO_NS::RationalTime Player::Private::loopPlayback(const OTIO_NS::RationalTime& time, bool& looped)
    {
        OTIO_NS::RationalTime out = time;
        looped = false;

        const auto& range = inOutRange->get();
        switch (loop->get())
        {
        case Loop::Loop:
        {
            out = tl::loop(out, range, &looped);
            if (looped)
            {
                resetPlaybackTime(out);
            }
            break;
        }
        case Loop::Once:
        {
            const auto playbackValue = playback->get();
            if (out < range.start_time() && Playback::Reverse == playbackValue)
            {
                out = range.start_time();
                playback->setIfChanged(Playback::Stop);
                {
                    std::unique_lock<std::mutex> lock(mutex.mutex);
                    mutex.state.playback = Playback::Stop;
                    mutex.clearRequests = true;
                }
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    audioMutex.state.playback = Playback::Stop;
                }
            }
            else if (out > range.end_time_inclusive() && Playback::Forward == playbackValue)
            {
                out = range.end_time_inclusive();
                playback->setIfChanged(Playback::Stop);
                {
                    std::unique_lock<std::mutex> lock(mutex.mutex);
                    mutex.state.playback = Playback::Stop;
                    mutex.clearRequests = true;
                }
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    audioMutex.state.playback = Playback::Stop;
                }
            }
            break;
        }
        case Loop::PingPong:
        {
            const auto playbackValue = playback->get();
            if (out < range.start_time() && Playback::Reverse == playbackValue)
            {
                out = range.start_time();
                playback->setIfChanged(Playback::Forward);
                {
                    std::unique_lock<std::mutex> lock(mutex.mutex);
                    mutex.state.playback = Playback::Forward;
                    mutex.state.currentTime = out;
                    mutex.clearRequests = true;
                    mutex.cacheDir = CacheDir::Forward;
                }
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    audioMutex.state.playback = Playback::Forward;
                    audioReset(out);
                }
                playbackReset(out);
            }
            else if (out > range.end_time_inclusive() && Playback::Forward == playbackValue)
            {
                out = range.end_time_inclusive();
                playback->setIfChanged(Playback::Reverse);
                {
                    std::unique_lock<std::mutex> lock(mutex.mutex);
                    mutex.state.playback = Playback::Reverse;
                    mutex.state.currentTime = out;
                    mutex.clearRequests = true;
                    mutex.cacheDir = CacheDir::Reverse;
                }
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    audioMutex.state.playback = Playback::Reverse;
                    audioReset(out);
                }
                playbackReset(out);
            }
            break;
        }
        default: break;
        }

        return out;
    }

    void Player::Private::clearRequests()
    {
        std::vector<std::vector<uint64_t> > ids(1 + thread.state.compare.size());
        for (const auto& i : thread.videoRequests)
        {
            for (size_t j = 0; j < i.second.size() && j < ids.size(); ++j)
            {
                ids[j].push_back(i.second[j].id);
            }
        }
        for (const auto& i : thread.audioRequests)
        {
            ids[0].push_back(i.second.id);
        }
        timeline->cancelRequests(ids[0]);
        for (size_t i = 0; i < thread.state.compare.size(); ++i)
        {
            thread.state.compare[i]->cancelRequests(ids[i + 1]);
        }
        thread.videoRequests.clear();
        thread.audioRequests.clear();
    }

    void Player::Private::clearCache()
    {
        thread.videoCache.clear();
        {
            std::unique_lock<std::mutex> lock(mutex.mutex);
            mutex.cacheInfo = PlayerCacheInfo();
        }
        {
            std::unique_lock<std::mutex> lock(audioMutex.mutex);
            audioMutex.cache.clear();
        }
    }

    bool Player::Private::hasVideo() const
    {
        // Asked of the timeline so that it follows the media reference key;
        // see getVideoCacheMax().
        return !timeline->getIOInfo().video.empty();
    }

    size_t Player::Private::getVideoCacheMax() const
    {
        // This function returns the approximate number of video frames
        // that can fit in the cache. Note that this doesn't take into
        // account clips with different sizes or multiple tracks.
        size_t byteCount = 0;
        const IOInfo& videoInfo = timeline->getIOInfo();
        if (thread.state.videoLayer >= 0 &&
            thread.state.videoLayer < static_cast<int>(videoInfo.video.size()))
        {
            byteCount += videoInfo.video[thread.state.videoLayer].getByteCount();

            // Add byte counts from timelines that are being compared.
            for (size_t i = 0; i < thread.state.compare.size(); ++i)
            {
                const int compareLayer = i < thread.state.compareVideoLayers.size() ?
                    thread.state.compareVideoLayers[i] :
                    thread.state.videoLayer;
                const IOInfo& compareInfo = thread.state.compare[i]->getIOInfo();
                if (compareLayer >= 0 &&
                    compareLayer < static_cast<int>(compareInfo.video.size()))
                {
                    byteCount += compareInfo.video[compareLayer].getByteCount();
                }
            }
        }

        return byteCount > 0 ?
            ((thread.state.cacheOptions.videoGB * ftk::gigabyte) / byteCount) :
            0;
    }

    size_t Player::Private::getAudioCacheMax() const
    {
        // This function returns the approximate number seconds of audio
        // that can fit in the cache. Note that this doesn't take into
        // account clips with different sizes or multiple tracks.
        const size_t byteCount =
            sourceAudioInfo.sampleRate * sourceAudioInfo.getByteCount();

        return byteCount > 0 ?
            ((thread.state.cacheOptions.audioGB * ftk::gigabyte) / byteCount) :
            0;
    }

    OTIO_NS::TimeRange Player::Private::getVideoCacheRange(size_t max) const
    {
        OTIO_NS::TimeRange out;

        // Clamp the read behind and read ahead so that the total cache
        // range, including the current frame, fits within the maximum
        // size.
        const double rate = thread.state.currentTime.rate();
        const double behindFrames = std::min(
            OTIO_NS::RationalTime(thread.state.cacheOptions.readBehind, 1.0).
                rescaled_to(rate).round().value(),
            // No more than a quarter of the cache: against a budget of
            // only a couple dozen frames, a fixed half second behind
            // would leave almost nothing ahead.
            max > 0 ? std::floor((max - 1) / 4.0) : 0.0);
        const double aheadFrames = std::max(
            static_cast<double>(max) - behindFrames - 1.0,
            0.0);
        const OTIO_NS::RationalTime readBehind(behindFrames, rate);
        const OTIO_NS::RationalTime readAhead = std::min(
            OTIO_NS::RationalTime(aheadFrames, rate),
            thread.state.inOutRange.duration());

        switch (thread.cacheDir)
        {
        case CacheDir::Forward:
            out = OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                (thread.state.currentTime - readBehind).round(),
                (thread.state.currentTime + readAhead).round());
            break;
        case CacheDir::Reverse:
            out = OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                (thread.state.currentTime - readAhead).round(),
                (thread.state.currentTime + readBehind).round());
            break;
        default: break;
        }

        return out;
    }

    ftk::Range<int64_t> Player::Private::getAudioCacheRange(size_t max) const
    {
        ftk::Range<int64_t> out;

        // Clamp the read behind and read ahead so that the total cache
        // range, including the current second, fits within the maximum
        // size.
        const int64_t c = thread.state.currentTime.rescaled_to(1.0).value();
        const int64_t readBehind = std::min(
            static_cast<int64_t>(
                OTIO_NS::RationalTime(thread.state.cacheOptions.readBehind, 1.0).value()),
            max > 0 ? static_cast<int64_t>(max - 1) : 0);
        const int64_t readAhead = std::min(
            std::max(static_cast<int64_t>(max) - readBehind - 1, static_cast<int64_t>(0)),
            static_cast<int64_t>(thread.state.inOutRange.duration().rescaled_to(1.0).value()));

        switch (thread.cacheDir)
        {
        case CacheDir::Forward:
            out = ftk::Range<int64_t>(c - readBehind, c + readAhead);
            break;
        case CacheDir::Reverse:
            out = ftk::Range<int64_t>(c - readAhead, c + readBehind);
            break;
        default: break;
        }

        return out;
    }

    bool Player::Private::PlaybackState::operator == (
        const PlaybackState& other) const
    {
        return
            playback == other.playback &&
            currentTime == other.currentTime &&
            inOutRange == other.inOutRange &&
            compare == other.compare &&
            compareTime == other.compareTime &&
            ioOptions == other.ioOptions &&
            videoLayer == other.videoLayer &&
            compareVideoLayers == other.compareVideoLayers &&
            audioOffset == other.audioOffset &&
            cacheOptions == other.cacheOptions;
    }

    bool Player::Private::PlaybackState::operator != (
        const PlaybackState& other) const
    {
        return !(*this == other);
    }

    void Player::Private::cacheEvictAndFill()
    {
        const size_t videoCacheMax = getVideoCacheMax();
        const size_t audioCacheMax = getAudioCacheMax();
        const OTIO_NS::TimeRange videoCacheRange = getVideoCacheRange(videoCacheMax);
        const ftk::Range<int64_t> audioCacheRange = getAudioCacheRange(audioCacheMax);

        // Remove frames from the video cache.
        {
            const auto looped = tl::loop(
                videoCacheRange,
                thread.state.inOutRange);
            auto i = thread.videoCache.begin();
            while (i != thread.videoCache.end())
            {
                const OTIO_NS::RationalTime& t = i->first;
                bool found = false;
                for (const auto& range : looped)
                {
                    if (range.contains(t))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    i = thread.videoCache.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        // Remove frames from the audio cache.
        {
            const auto looped = tl::loop(
                audioCacheRange,
                ftk::Range<int64_t>(
                    thread.state.inOutRange.start_time().rescaled_to(1.0).value(),
                    thread.state.inOutRange.end_time_inclusive().rescaled_to(1.0).value()));
            std::unique_lock<std::mutex> lock(audioMutex.mutex);
            auto i = audioMutex.cache.begin();
            while (i != audioMutex.cache.end())
            {
                const int64_t seconds = i->first;
                bool found = false;
                for (const auto& range : looped)
                {
                    if (seconds >= range.min() && seconds <= range.max())
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    i = audioMutex.cache.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        // Fill the video cache.
        if (hasVideo())
        {
            const OTIO_NS::RationalTime inc(1.0, thread.state.currentTime.rate());
            for (OTIO_NS::RationalTime time = videoCacheRange.start_time();
                time <= videoCacheRange.end_time_inclusive() &&
                thread.videoRequests.size() < playerOptions.videoRequestMax;
                time += inc)
            {
                const OTIO_NS::RationalTime timeLooped = tl::loop(time, thread.state.inOutRange);
                const auto j = thread.videoCache.find(timeLooped);
                if (j == thread.videoCache.end())
                {
                    const auto k = thread.videoRequests.find(timeLooped);
                    if (k == thread.videoRequests.end())
                    {
                        auto& requests = thread.videoRequests[timeLooped];
                        IOOptions ioOptions2 = thread.state.ioOptions;
                        ioOptions2["Layer"] = ftk::Format("{0}").arg(thread.state.videoLayer);
                        const IOOptions ioOptionsA = ioOptions2;
                        requests.clear();
                        requests.push_back(timeline->getVideo(timeLooped, ioOptions2));

                        for (size_t k = 0; k < thread.state.compare.size(); ++k)
                        {
                            const OTIO_NS::TimeRange compareTimeRange =
                                thread.state.compare[k]->getTimeRange();
                            const OTIO_NS::TimeRange compareInOutRange =
                                k < thread.state.compareInOutRanges.size() &&
                                !tl::compareExact(
                                    thread.state.compareInOutRanges[k],
                                    invalidTimeRange) ?
                                thread.state.compareInOutRanges[k] :
                                compareTimeRange;
                            const OTIO_NS::RationalTime t2 = tl::getCompareTime(
                                timeLooped,
                                timeRange,
                                compareTimeRange,
                                thread.state.compareTime,
                                thread.state.inOutRange,
                                compareInOutRange,
                                thread.state.compareTimeOptions);
                            ioOptions2["Layer"] = ftk::Format("{0}").
                                arg(k < thread.state.compareVideoLayers.size() ?
                                    thread.state.compareVideoLayers[k] :
                                    thread.state.videoLayer);

                            // A file compared with itself is the same
                            // timeline asked for the same frame twice. The
                            // movie readers hand out frames in the order
                            // they were asked for and start the decoder
                            // again whenever the time is not the one they
                            // expected, so the second ask restarts it for
                            // every frame -- playback fell to about one
                            // frame a second. It is the same frame: leave a
                            // request without a future and take a copy of
                            // the first one when it arrives.
                            if (thread.state.compare[k] == timeline &&
                                t2.strictly_equal(timeLooped) &&
                                ioOptions2 == ioOptionsA)
                            {
                                requests.push_back(VideoRequest());
                            }
                            else
                            {
                                requests.push_back(
                                    thread.state.compare[k]->getVideo(t2, ioOptions2));
                            }
                        }
                    }
                }
            }
        }

        // Fill the audio cache.
        if (sourceAudioInfo.isValid())
        {
            for (int64_t seconds = audioCacheRange.min();
                seconds <= audioCacheRange.max() &&
                thread.audioRequests.size() < playerOptions.audioRequestMax;
                ++seconds)
            {
                const int64_t secondsLooped = tl::loop(seconds + thread.state.audioOffset, thread.state.inOutRange);
                bool found = false;
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    const auto j = audioMutex.cache.find(secondsLooped);
                    found = j != audioMutex.cache.end();
                }
                if (!found)
                {
                    const auto j = thread.audioRequests.find(secondsLooped);
                    if (j == thread.audioRequests.end())
                    {
                        auto& request = thread.audioRequests[secondsLooped];
                        request = timeline->getAudio(secondsLooped, thread.state.ioOptions);
                    }
                }
            }
        }
    }

    void Player::Private::cacheUpdate()
    {

        // Stopped on a filled cache this decides, two hundred times a
        // second, that there is nothing to do: evicting walks the whole
        // cache and filling walks the whole range to find that out. The key
        // holds everything those two read, so an unchanged key means an
        // unchanged answer. Completing a request or moving the playhead
        // changes a size or the state, which is what puts the work back.
        size_t audioCacheSize = 0;
        {
            std::unique_lock<std::mutex> lock(audioMutex.mutex);
            audioCacheSize = audioMutex.cache.size();
        }
        // Compared in place rather than into a key: holding the state costs
        // a copy of its options and layers, which is not worth paying on a
        // tick that turns out to have nothing to do.
        const bool changed =
            !thread.cacheKeyValid ||
            thread.cacheKey.cacheDir != thread.cacheDir ||
            thread.cacheKey.videoCacheSize != thread.videoCache.size() ||
            thread.cacheKey.audioCacheSize != audioCacheSize ||
            thread.cacheKey.videoRequestsSize != thread.videoRequests.size() ||
            thread.cacheKey.audioRequestsSize != thread.audioRequests.size() ||
            thread.cacheKey.state != thread.state;
        if (changed)
        {
            cacheEvictAndFill();

            thread.cacheKey.state = thread.state;
            thread.cacheKey.cacheDir = thread.cacheDir;
            thread.cacheKey.videoCacheSize = thread.videoCache.size();
            {
                std::unique_lock<std::mutex> lock(audioMutex.mutex);
                thread.cacheKey.audioCacheSize = audioMutex.cache.size();
            }
            thread.cacheKey.videoRequestsSize = thread.videoRequests.size();
            thread.cacheKey.audioRequestsSize = thread.audioRequests.size();
            thread.cacheKeyValid = true;
        }

        // Check for finished video.
        auto videoRequestsIt = thread.videoRequests.begin();
        while (videoRequestsIt != thread.videoRequests.end())
        {
            bool ready = true;
            for (auto videoRequestIt = videoRequestsIt->second.begin();
                videoRequestIt != videoRequestsIt->second.end();
                ++videoRequestIt)
            {
                // A request without a future is one that was not made,
                // because it would have asked for a frame already being
                // read; it is filled in below.
                if (videoRequestIt->future.valid())
                {
                    ready &= videoRequestIt->future.wait_for(std::chrono::seconds(0)) ==
                        std::future_status::ready;
                }
            }
            if (ready)
            {
                const OTIO_NS::RationalTime time = videoRequestsIt->first;
                std::vector<VideoFrame> videoFrameList;
                for (auto videoRequestIt = videoRequestsIt->second.begin();
                    videoRequestIt != videoRequestsIt->second.end();
                    ++videoRequestIt)
                {
                    VideoFrame videoFrame;
                    if (videoRequestIt->future.valid())
                    {
                        videoFrame = videoRequestIt->future.get();
                    }
                    else if (!videoFrameList.empty())
                    {
                        videoFrame = videoFrameList.front();
                    }
                    videoFrame.time = time;
                    videoFrameList.emplace_back(videoFrame);
                }
                thread.videoCache[time] = videoFrameList;
                videoRequestsIt = thread.videoRequests.erase(videoRequestsIt);
            }
            else
            {
                ++videoRequestsIt;
            }
        }

        // Check for finished audio.
        auto audioRequestsIt = thread.audioRequests.begin();
        while (audioRequestsIt != thread.audioRequests.end())
        {
            const int64_t s = audioRequestsIt->first;
            if (audioRequestsIt->second.future.valid() &&
                audioRequestsIt->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                auto audioFrame = audioRequestsIt->second.future.get();
                audioFrame.seconds = s;
                {
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    audioMutex.cache[audioRequestsIt->first] = audioFrame;
                }
                audioRequestsIt = thread.audioRequests.erase(audioRequestsIt);
            }
            else
            {
                ++audioRequestsIt;
            }
        }

        // Update cache information.
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> diff = now - thread.cacheTimer;
        if (diff.count() > .5F)
        {
            thread.cacheTimer = now;

            const size_t videoCacheMax = getVideoCacheMax();
            const size_t audioCacheMax = getAudioCacheMax();

            std::vector<OTIO_NS::RationalTime> videoCacheFrames;
            for (const auto& i : thread.videoCache)
            {
                videoCacheFrames.push_back(i.first);
            }
            const float videoCachePercentage = videoCacheMax > 0 ?
                (videoCacheFrames.size() / static_cast<float>(videoCacheMax) * 100.F) :
                0.F;

            std::vector<int64_t> audioCacheKeys;
            {
                std::unique_lock<std::mutex> lock(audioMutex.mutex);
                for (const auto& i : audioMutex.cache)
                {
                    audioCacheKeys.push_back(i.first);
                }
            }
            std::vector<OTIO_NS::RationalTime> audioCacheFrames;
            for (const auto& key : audioCacheKeys)
            {
                audioCacheFrames.push_back(OTIO_NS::RationalTime(key, 1.0));
            }
            const float audioCachePercentage = audioCacheMax > 0 ?
                (audioCacheKeys.size() / static_cast<float>(audioCacheMax) * 100.F) :
                0.F;

            auto videoCacheRanges = toRanges(videoCacheFrames);
            auto audioCacheRanges = toRanges(audioCacheFrames);
            for (auto& i : audioCacheRanges)
            {
                i = OTIO_NS::TimeRange(
                    i.start_time().rescaled_to(timeRange.duration().rate()).floor(),
                    i.duration().rescaled_to(timeRange.duration().rate()).ceil());
            }
            {
                std::unique_lock<std::mutex> lock(mutex.mutex);
                mutex.cacheInfo.videoPercentage = videoCachePercentage;
                mutex.cacheInfo.audioPercentage = audioCachePercentage;
                mutex.cacheInfo.video = videoCacheRanges;
                mutex.cacheInfo.audio = audioCacheRanges;
            }
        }
    }

    void Player::Private::playbackReset(const OTIO_NS::RationalTime& time)
    {
        std::unique_lock<std::mutex> lock(noAudio.mutex);
        noAudio.playbackTimer = std::chrono::steady_clock::now();
        noAudio.start = time;
    }

    void Player::Private::resetPlaybackTime(const OTIO_NS::RationalTime& time)
    {
        // Only seek and loop wrap call this, so it is the choke point for
        // genuine position discontinuities -- the jumps the dropped-frame
        // detector must absorb rather than count. The stall re-sync resets the
        // clock through playbackReset() directly and deliberately does NOT land
        // here, so a sustained stall keeps counting the frames it skips.
        droppedFramesReset = true;

        // Reset both playback clocks. The audio-sample clock (audioMutex) is
        // read by _tick only when an audio device is open; the wall clock
        // (noAudio) only when it is not. Resetting the inactive one is a
        // harmless dead write, which lets the timing paths avoid branching on
        // hasAudio() at every call site.
        {
            std::unique_lock<std::mutex> lock(audioMutex.mutex);
            audioReset(time);
        }
        playbackReset(time);
    }

    void Player::Private::droppedFramesTick(
        const std::optional<OTIO_NS::RationalTime>& presentedTime,
        Playback playback,
        double timelineSpeed)
    {
        const OTIO_NS::RationalTime clockTime = currentTime->get();

        // (Re)establish the baseline after a discontinuity (seek, loop wrap,
        // play start) or on the first tick. The running total carries across so
        // loops keep accumulating; the clock anchor and shown-count restart so
        // the discontinuity jump itself is not counted.
        if (droppedFramesReset.exchange(false) || !droppedClockBaseline.has_value())
        {
            droppedClockBaseline = clockTime;
            droppedPresentedLast = presentedTime;
            droppedShownCount = 0;
            droppedPeak = 0;
            droppedBase = droppedFrames->get();
            return;
        }

        // Count each distinct frame actually shown since the baseline.
        if (presentedTime.has_value() && presentedTime != droppedPresentedLast)
        {
            droppedPresentedLast = presentedTime;
            ++droppedShownCount;
        }

        // Only meaningful at real-time or slower playback. Faster speeds (the
        // accelerate feature or a custom fast speed) skip source frames by
        // design, which is not the same as the engine failing to keep up.
        const double effectiveRate = speed->get() * speedMult->get();
        if (effectiveRate <= timelineSpeed * 1.001)
        {
            const double dir = (Playback::Reverse == playback) ? -1.0 : 1.0;
            const int64_t clockAdvance = static_cast<int64_t>(std::llround(
                (clockTime - droppedClockBaseline.value()).rescaled_to(timelineSpeed).value() * dir));

            // Frames the clock passed but never showed. The -2 absorbs the
            // in-flight current frame plus the normal sub-frame presentation
            // lag, so steady playback reads zero. droppedPeak only grows, so the
            // count never ticks backwards when presentation briefly catches up.
            const int64_t deficit = clockAdvance - droppedShownCount - 2;
            if (deficit > droppedPeak)
            {
                droppedPeak = deficit;
                droppedFrames->setIfChanged(droppedBase + static_cast<size_t>(droppedPeak));
            }
        }
    }

    void Player::Private::log()
    {
        if (auto logSystem = this->logSystem.lock())
        {
            const std::string id = ftk::Format("tl::Player {0}").arg(this);

            // Get values.
            OTIO_NS::RationalTime currentTime;
            OTIO_NS::TimeRange inOutRange;
            IOOptions ioOptions;
            PlayerCacheInfo cacheInfo;
            {
                std::unique_lock<std::mutex> lock(mutex.mutex);
                currentTime = mutex.state.currentTime;
                inOutRange = mutex.state.inOutRange;
                ioOptions = mutex.state.ioOptions;
                cacheInfo = mutex.cacheInfo;
            }
            const size_t videoCacheMax = getVideoCacheMax();
            const size_t videoCacheSize = thread.videoCache.size();
            size_t audioCacheMax = getAudioCacheMax();
            size_t audioCacheSize = 0;
            {
                std::unique_lock<std::mutex> lock(audioMutex.mutex);
                audioCacheSize = audioMutex.cache.size();
            }

            // Create an array of characters to draw the timeline.
            const size_t lineLength = 80;
            std::string currentTimeDisplay(lineLength, '.');
            double n = (currentTime - timeRange.start_time()).value() / timeRange.duration().value();
            size_t index = ftk::clamp(n, 0.0, 1.0) * (lineLength - 1);
            if (index < currentTimeDisplay.size())
            {
                currentTimeDisplay[index] = 'T';
            }

            // Create an array of characters to draw the cached video frames.
            std::string cachedVideoFramesDisplay(lineLength, '.');
            for (const auto& i : cacheInfo.video)
            {
                n = (i.start_time() - timeRange.start_time()).value() / timeRange.duration().value();
                const size_t t0 = ftk::clamp(n, 0.0, 1.0) * (lineLength - 1);
                n = (i.end_time_inclusive() - timeRange.start_time()).value() / timeRange.duration().value();
                const size_t t1 = ftk::clamp(n, 0.0, 1.0) * (lineLength - 1);
                for (size_t j = t0; j <= t1; ++j)
                {
                    if (j < cachedVideoFramesDisplay.size())
                    {
                        cachedVideoFramesDisplay[j] = 'V';
                    }
                }
            }

            // Create an array of characters to draw the cached audio frames.
            std::string cachedAudioFramesDisplay(lineLength, '.');
            for (const auto& i : cacheInfo.audio)
            {
                double n = (i.start_time() - timeRange.start_time()).value() / timeRange.duration().value();
                const size_t t0 = ftk::clamp(n, 0.0, 1.0) * (lineLength - 1);
                n = (i.end_time_inclusive() - timeRange.start_time()).value() / timeRange.duration().value();
                const size_t t1 = ftk::clamp(n, 0.0, 1.0) * (lineLength - 1);
                for (size_t j = t0; j <= t1; ++j)
                {
                    if (j < cachedAudioFramesDisplay.size())
                    {
                        cachedAudioFramesDisplay[j] = 'A';
                    }
                }
            }

            std::vector<std::string> ioOptionStrings;
            for (const auto& i : ioOptions)
            {
                ioOptionStrings.push_back(ftk::Format("{0}:{1}").arg(i.first).arg(i.second));
            }

            logSystem->print(id, ftk::Format(
                "\n"
                "    * Path: {0}\n"
                "    * Current time: {1}\n"
                "    * In/out range: {2}\n"
                "    * I/O options: {3}\n"
                "    * Video cache: {4}% {5}GB\n"
                "    * Audio cache: {6}% {7}GB\n"
                "    * Read behind: {8} seconds\n"
                "    * Video requests: {9}\n"
                "    * Audio requests: {10}\n"
                "    {11}\n"
                "    {12}\n"
                "    {13}\n"
                "    (T=current time, V=cached video, A=cached audio)").
                arg(timeline->getPath().get()).
                arg(currentTime).
                arg(inOutRange).
                arg(ftk::join(ioOptionStrings, ", ")).
                arg(videoCacheMax > 0 ? (videoCacheSize / static_cast<double>(videoCacheMax) * 100.0) : 0.0).
                arg(cacheOptions->get().videoGB).
                arg(audioCacheMax > 0 ? (audioCacheSize / static_cast<double>(audioCacheMax) * 100.0) : 0.0).
                arg(cacheOptions->get().audioGB).
                arg(cacheOptions->get().readBehind).
                arg(thread.videoRequests.size()).
                arg(thread.audioRequests.size()).
                arg(currentTimeDisplay).
                arg(cachedVideoFramesDisplay).
                arg(cachedAudioFramesDisplay));
        }
    }
}
