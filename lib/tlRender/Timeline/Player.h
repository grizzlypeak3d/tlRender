// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/PlayerOptions.h>
#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/ObservableList.h>

namespace tl
{
    class System;

    //! Timeline player cache information.
    struct TL_TIMELINE_API_TYPE PlayerCacheInfo
    {
        //! Percentage used of the video cache.
        float videoPercentage = 0.F;

        //! Percentage used of the audio cache.
        float audioPercentage = 0.F;

        //! Cached video.
        std::vector<OTIO_NS::TimeRange> video;

        //! Cached audio.
        std::vector<OTIO_NS::TimeRange> audio;

        TL_TIMELINE_API bool operator == (const PlayerCacheInfo&) const;
        TL_TIMELINE_API bool operator != (const PlayerCacheInfo&) const;
    };

    //! Playback modes.
    enum class TL_TIMELINE_API_TYPE Playback
    {
        Stop,
        Forward,
        Reverse,

        Count,
        First = Stop
    };
    FTK_ENUM(TL_TIMELINE_API, Playback);

    //! Playback loop modes.
    enum class TL_TIMELINE_API_TYPE Loop
    {
        Loop,
        Once,
        PingPong,

        Count,
        First = Loop
    };
    FTK_ENUM(TL_TIMELINE_API, Loop);

    //! Time actions.
    enum class TL_TIMELINE_API_TYPE TimeAction
    {
        Start,
        End,
        FramePrev,
        FramePrevX10,
        FramePrevX100,
        FrameNext,
        FrameNextX10,
        FrameNextX100,
        JumpBack1s,
        JumpBack10s,
        JumpForward1s,
        JumpForward10s,

        Count,
        First = Start
    };
    FTK_ENUM(TL_TIMELINE_API, TimeAction);

    //! Timeline player.
    class TL_TIMELINE_API_TYPE Player : public std::enable_shared_from_this<Player>
    {
        FTK_NON_COPYABLE(Player);

    protected:
        void _init(
            const std::shared_ptr<ftk::Context>&,
            const std::shared_ptr<Timeline>&,
            const PlayerOptions&);

        Player();

    public:
        TL_TIMELINE_API ~Player();

        //! Create a new timeline player.
        TL_TIMELINE_API static std::shared_ptr<Player> create(
            const std::shared_ptr<ftk::Context>&,
            const std::shared_ptr<Timeline>&,
            const PlayerOptions& = PlayerOptions());

        //! Get the context.
        TL_TIMELINE_API std::shared_ptr<ftk::Context> getContext() const;

        //! Get the timeline.
        TL_TIMELINE_API const std::shared_ptr<Timeline>& getTimeline() const;

        //! Get the path.
        TL_TIMELINE_API const ftk::Path& getPath() const;

        //! Get the audio path.
        TL_TIMELINE_API const ftk::Path& getAudioPath() const;

        //! Get the timeline player options.
        TL_TIMELINE_API const PlayerOptions& getPlayerOptions() const;

        //! Get the timeline options.
        TL_TIMELINE_API const Options& getOptions() const;

        //! \name Information
        ///@{

        //! Get the time range.
        TL_TIMELINE_API const OTIO_NS::TimeRange& getTimeRange() const;

        //! Get the duration.
        TL_TIMELINE_API OTIO_NS::RationalTime getDuration() const;

        //! Get the I/O information. The information is retrieved from
        //! the first clip in the timeline.
        TL_TIMELINE_API const IOInfo& getIOInfo() const;

        ///@}

        //! \name Playback
        ///@{

        //! Get the default playback speed.
        TL_TIMELINE_API double getDefaultSpeed() const;

        //! Get the playback speed.
        TL_TIMELINE_API double getSpeed() const;

        //! Observe the playback speed.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<double> > observeSpeed() const;

        //! Set the playback speed.
        TL_TIMELINE_API void setSpeed(double);

        //! Get the playback speed multiplier.
        TL_TIMELINE_API double getSpeedMult() const;

        //! Observe the playback speed multiplier.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<double> > observeSpeedMult() const;

        //! Set the playback speed multiplier.
        TL_TIMELINE_API void setSpeedMult(double);

        //! Get the actual playback speed (speed * speed multiplier).
        TL_TIMELINE_API double getActualSpeed() const;

        //! Observe the actual playback speed (speed * speed multiplier).
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<double> > observeActualSpeed() const;

        //! Get the number of frames dropped during playback. This counts the
        //! frames the engine skipped to keep in sync with the playback clock
        //! (decode or I/O could not keep up); it is measured only at real-time
        //! or slower speeds, and is reset when playback starts or stops.
        TL_TIMELINE_API size_t getDroppedFrames() const;

        //! Observe the number of frames dropped during playback.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<size_t> > observeDroppedFrames() const;

        //! Get the playback mode.
        TL_TIMELINE_API Playback getPlayback() const;

        //! Observe the playback mode.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<Playback> > observePlayback() const;

        //! Set the playback mode.
        TL_TIMELINE_API void setPlayback(Playback);

        //! Toggle the playback mode.
        TL_TIMELINE_API void togglePlayback();

        //! Get whether playback is stopped.
        TL_TIMELINE_API bool isStopped() const;

        //! Stop playback.
        TL_TIMELINE_API void stop();

        //! Start forward playback.
        TL_TIMELINE_API void forward();

        //! Start reverse playback.
        TL_TIMELINE_API void reverse();

        //! Get the playback loop.
        TL_TIMELINE_API Loop getLoop() const;

        //! Observe the playback loop mode.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<Loop> > observeLoop() const;

        //! Set the playback loop mode.
        TL_TIMELINE_API void setLoop(Loop);

        ///@}

        //! \name Time
        ///@{

        //! Get the current time.
        TL_TIMELINE_API const OTIO_NS::RationalTime& getCurrentTime() const;

        //! Observe the current time.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<OTIO_NS::RationalTime> > observeCurrentTime() const;

        //! Observe seeking.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<OTIO_NS::RationalTime> > observeSeek() const;

        //! Seek to the given time.
        TL_TIMELINE_API void seek(const OTIO_NS::RationalTime&);

        //! Time action.
        TL_TIMELINE_API void timeAction(TimeAction);

        //! Go to the start time.
        TL_TIMELINE_API void gotoStart();

        //! Go to the end time.
        TL_TIMELINE_API void gotoEnd();

        //! Go to the previous frame.
        TL_TIMELINE_API void framePrev();

        //! Go to the next frame.
        TL_TIMELINE_API void frameNext();

        ///@}

        //! \name In/Out Points
        ///@{

        //! Get the in/out points range.
        TL_TIMELINE_API const OTIO_NS::TimeRange& getInOutRange() const;

        //! Observe the in/out points range.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<OTIO_NS::TimeRange> > observeInOutRange() const;

        //! Set the in/out points range.
        TL_TIMELINE_API void setInOutRange(const OTIO_NS::TimeRange&);

        //! Set the in point to the current time.
        TL_TIMELINE_API void setInPoint();

        //! Reset the in point
        TL_TIMELINE_API void resetInPoint();

        //! Set the out point to the current time.
        TL_TIMELINE_API void setOutPoint();

        //! Reset the out point
        TL_TIMELINE_API void resetOutPoint();

        ///@}

        //! \name Comparison
        ///@{

        //! Get the timelines for comparison.
        TL_TIMELINE_API const std::vector<std::shared_ptr<Timeline> >& getCompare() const;

        //! Observe the timelines for comparison.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<std::shared_ptr<Timeline> > > observeCompare() const;

        //! Set the timelines for comparison.
        TL_TIMELINE_API void setCompare(const std::vector<std::shared_ptr<Timeline> >&);

        //! Get the selected in/out ranges for the comparison timelines.
        TL_API const std::vector<OTIO_NS::TimeRange>& getCompareInOutRanges() const;

        //! Observe the selected in/out ranges for the comparison timelines.
        TL_API std::shared_ptr<ftk::IObservable<std::vector<OTIO_NS::TimeRange> > > observeCompareInOutRanges() const;

        //! Set the selected in/out ranges for the comparison timelines.
        TL_API void setCompareInOutRanges(const std::vector<OTIO_NS::TimeRange>&);

        //! Get the comparison time mode.
        TL_TIMELINE_API CompareTime getCompareTime() const;

        //! Observe the comparison time mode.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<CompareTime> > observeCompareTime() const;

        //! Set the comparison time mode.
        TL_TIMELINE_API void setCompareTime(CompareTime);

        //! Get the relative comparison time options.
        TL_API const CompareTimeOptions& getCompareTimeOptions() const;

        //! Observe the relative comparison time options.
        TL_API std::shared_ptr<ftk::IObservable<CompareTimeOptions> > observeCompareTimeOptions() const;

        //! Set the relative comparison time options.
        TL_API void setCompareTimeOptions(const CompareTimeOptions&);

        ///@}

        //! \name I/O
        ///@{

        //! Get the I/O options.
        TL_TIMELINE_API const IOOptions& getIOOptions() const;

        //! Observe the I/O options.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<IOOptions> > observeIOOptions() const;

        //! Set the I/O options.
        TL_TIMELINE_API void setIOOptions(const IOOptions&);

        ///@}

        //! \name Media References
        ///
        //! Clips may carry several media references, for example a proxy and a
        //! full resolution version of the same media, selected by key. The key
        //! is applied to the timeline and to any comparison timelines.
        ///@{

        //! Get the media reference key. An empty key, the default, leaves
        //! every clip on the media reference that OTIO has active.
        TL_TIMELINE_API const std::string& getMediaReferenceKey() const;

        //! Observe the media reference key.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<std::string> > observeMediaReferenceKey() const;

        //! Set the media reference key. Clips that have no media reference
        //! with this key fall back to OTIO_NS::Clip::default_media_key.
        //!
        //! The cache is cleared so that the media already read is replaced.
        TL_TIMELINE_API void setMediaReferenceKey(const std::string&);

        //! Get the media reference keys used by the timeline and any
        //! comparison timelines, sorted and without duplicates.
        TL_TIMELINE_API std::vector<std::string> getMediaReferenceKeys() const;

        ///@}

        //! \name Video
        ///@{

        //! Get the video layer.
        TL_TIMELINE_API int getVideoLayer() const;

        //! Observer the video layer.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<int> > observeVideoLayer() const;

        //! Set the video layer.
        TL_TIMELINE_API void setVideoLayer(int);

        //! Get the comparison video layers.
        TL_TIMELINE_API const std::vector<int>& getCompareVideoLayers() const;

        //! Observe the comparison video layers.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<int> > observeCompareVideoLayers() const;

        //! Set the comparison video layers.
        TL_TIMELINE_API void setCompareVideoLayers(const std::vector<int>&);

        //! Get the current video frame.
        TL_TIMELINE_API const std::vector<VideoFrame>& getCurrentVideo() const;

        //! Observe the current video frame.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<VideoFrame> > observeCurrentVideo() const;

        ///@}

        //! \name Audio
        ///@{

        //! Get the audio device.
        TL_TIMELINE_API const AudioDeviceID& getAudioDevice() const;

        //! Observe the audio devices.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<AudioDeviceID> > observeAudioDevice() const;

        //! Set the audio device.
        TL_TIMELINE_API void setAudioDevice(const AudioDeviceID&);

        //! Get the volume.
        TL_TIMELINE_API float getVolume() const;

        //! Observe the audio volume.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<float> > observeVolume() const;

        //! Set the audio volume.
        TL_TIMELINE_API void setVolume(float);

        //! Get the audio mute.
        TL_TIMELINE_API bool isMuted() const;

        //! Observe the audio mute.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<bool> > observeMute() const;

        //! Set the audio mute.
        TL_TIMELINE_API void setMute(bool);

        //! Get the audio channels mute.
        TL_TIMELINE_API const std::vector<bool>& getChannelMute() const;

        //! Observe the audio channels mute.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<bool> > observeChannelMute() const;

        //! Set the audio channels mute.
        TL_TIMELINE_API void setChannelMute(const std::vector<bool>&);

        //! Get the audio sync offset (in seconds).
        TL_TIMELINE_API double getAudioOffset() const;

        //! Observe the audio sync offset (in seconds).
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<double> > observeAudioOffset() const;

        //! Set the audio sync offset (in seconds).
        TL_TIMELINE_API void setAudioOffset(double);

        //! Get the current audio frames.
        TL_TIMELINE_API const std::vector<AudioFrame>& getCurrentAudio() const;

        //! Observe the current audio frames.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<AudioFrame> > observeCurrentAudio() const;

        ///@}

        //! \name Cache
        ///@{

        //! Get the cache options.
        TL_TIMELINE_API const PlayerCacheOptions& getCacheOptions() const;

        //! Observe the cache options.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<PlayerCacheOptions> > observeCacheOptions() const;

        //! Set the cache options.
        TL_TIMELINE_API void setCacheOptions(const PlayerCacheOptions&);

        //! Observe the cache information.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<PlayerCacheInfo> > observeCacheInfo() const;

        //! Clear the cache.
        TL_TIMELINE_API void clearCache();

        ///@}

        //! Get the number of objects currenty instantiated.
        TL_TIMELINE_API static size_t getObjectCount();

    private:
        void _setSpeedMult(double);

        void _tick();
        void _thread();

        friend class System;

        FTK_PRIVATE();
    };
}
