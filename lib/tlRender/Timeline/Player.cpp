// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/PlayerPrivate.h>

#include <tlRender/Timeline/System.h>
#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Time.h>

namespace tl
{
    bool PlayerCacheInfo::operator == (const PlayerCacheInfo& other) const
    {
        return
            videoPercentage == other.videoPercentage &&
            audioPercentage == other.audioPercentage &&
            video == other.video &&
            audio == other.audio;
    }

    bool PlayerCacheInfo::operator != (const PlayerCacheInfo& other) const
    {
        return !(*this == other);
    }

    TL_ENUM_IMPL(
        Playback,
        "Stop",
        "Forward",
        "Reverse");

    TL_ENUM_IMPL(
        Loop,
        "Loop",
        "Once",
        "Ping-Pong");

    TL_ENUM_IMPL(TimeAction,
        "Start",
        "End",
        "Frame Previous",
        "Frame Previous X10",
        "Frame Previous X100",
        "Frame Next",
        "Frame Next X10",
        "Frame Next X100",
        "Jump Back 1s",
        "Jump Back 10s",
        "Jump Forward 1s",
        "Jump Forward 10s");

    void Player::_init(
        const std::shared_ptr<ftk::Context>& context,
        const std::shared_ptr<Timeline>& timeline,
        const PlayerOptions& playerOptions)
    {
        FTK_P();

        if (auto system = context->getSystem<System>())
        {
            system->_addPlayer(shared_from_this());
        }

        auto logSystem = context->getLogSystem();
        p.logSystem = logSystem;
        {
            std::vector<std::string> lines;
            lines.push_back(std::string());
            lines.push_back(ftk::Format("    * Video cache: {0}GB").
                arg(playerOptions.cache.videoGB));
            lines.push_back(ftk::Format("    * Audio cache: {0}GB").
                arg(playerOptions.cache.audioGB));
            lines.push_back(ftk::Format("    * Cache read behind: {0}").
                arg(playerOptions.cache.readBehind));
            lines.push_back(ftk::Format("    * Audio buffer frame count: {0}").
                arg(playerOptions.audioBufferFrameCount));
            lines.push_back(ftk::Format("    * Mute timeout: {0}ms").
                arg(playerOptions.muteTimeout.count()));
            lines.push_back(ftk::Format("    * Sleep timeout: {0}ms").
                arg(playerOptions.sleepTimeout.count()));
            logSystem->print(
                ftk::Format("tl::Player {0}").arg(this),
                ftk::join(lines, "\n"));
        }

        p.playerOptions = playerOptions;
        p.timeline = timeline;
        p.timeRange = timeline->getTimeRange();
        p.sourceAudioInfo = timeline->getIOInfo().audio;

        // Create observables.
        p.speed = ftk::Observable<double>::create(p.timeRange.duration().rate());
        p.speedMult = ftk::Observable<double>::create(1.0);
        p.actualSpeed = ftk::Observable<double>::create(p.timeRange.duration().rate());
        p.playback = ftk::Observable<Playback>::create(Playback::Stop);
        p.loop = ftk::Observable<Loop>::create(Loop::Loop);
        p.currentTime = ftk::Observable<OTIO_NS::RationalTime>::create(
            playerOptions.currentTime != invalidTime ?
            playerOptions.currentTime :
            p.timeRange.start_time());
        p.seek = ftk::Observable<OTIO_NS::RationalTime>::create(p.currentTime->get());
        p.inOutRange = ftk::Observable<OTIO_NS::TimeRange>::create(p.timeRange);
        p.compare = ftk::ObservableList<std::shared_ptr<Timeline> >::create();
        p.compareTime = ftk::Observable<CompareTime>::create(CompareTime::Relative);
        p.ioOptions = ftk::Observable<IOOptions>::create();
        p.mediaReferenceKey = ftk::Observable<std::string>::create(
            timeline->getMediaReferenceKey());
        p.videoLayer = ftk::Observable<int>::create(0);
        p.compareVideoLayers = ftk::ObservableList<int>::create();
        p.currentVideoFrame = ftk::ObservableList<VideoFrame>::create();
        p.audioDevice = ftk::Observable<AudioDeviceID>::create(playerOptions.audioDevice);
        p.volume = ftk::Observable<float>::create(1.F);
        p.mute = ftk::Observable<bool>::create(false);
        p.channelMute = ftk::ObservableList<bool>::create();
        p.audioOffset = ftk::Observable<double>::create(0.0);
        p.currentAudioFrame = ftk::ObservableList<AudioFrame>::create();
        p.cacheOptions = ftk::Observable<PlayerCacheOptions>::create(playerOptions.cache);
        p.cacheInfo = ftk::Observable<PlayerCacheInfo>::create();
        p.droppedFrames = ftk::Observable<size_t>::create(0);

        // Create observers.
        auto audioSystem = context->getSystem<AudioSystem>();
        auto weak = std::weak_ptr<Player>(shared_from_this());
        p.audioDevicesObserver = ftk::ListObserver<AudioDeviceInfo>::create(
            audioSystem->observeDevices(),
            [weak](const std::vector<AudioDeviceInfo>&)
            {
                if (auto player = weak.lock())
                {
                    if (auto context = player->getContext())
                    {
                        player->_p->audioInit(context);
                    }
                }
            },
            ftk::ObserverAction::Suppress);
        p.defaultAudioDeviceObserver = ftk::Observer<AudioDeviceInfo>::create(
            audioSystem->observeDefaultDevice(),
            [weak](const AudioDeviceInfo&)
            {
                if (auto player = weak.lock())
                {
                    if (AudioDeviceID() == player->_p->audioDevice->get())
                    {
                        if (auto context = player->getContext())
                        {
                            player->_p->audioInit(context);
                        }
                    }
                }
            },
            ftk::ObserverAction::Suppress);

        // Initialize the audio.
        p.audioInit(context);

        // Create a new thread.
        p.mutex.state.currentTime = p.currentTime->get();
        p.mutex.state.inOutRange = p.inOutRange->get();
        p.mutex.state.audioOffset = p.audioOffset->get();
        p.mutex.state.cacheOptions = p.cacheOptions->get();
        p.audioMutex.state.speed = p.speed->get() * p.speedMult->get();
        p.log();
        p.running = true;
        p.thread.thread = std::thread(
            [this]
            {
                _thread();
            });
    }

    namespace
    {
        std::atomic<size_t> objectCount = 0;
    }

    Player::Player() :
        _p(new Private)
    {
        ++objectCount;
    }

    Player::~Player()
    {
        FTK_P();
        if (auto logSystem = p.logSystem.lock())
        {
            logSystem->print(
                ftk::Format("tl::~Player {0}").arg(this),
                p.timeline->getPath().get());
        }

        p.running = false;
        if (p.thread.thread.joinable())
        {
            p.thread.thread.join();
        }

#if defined(FTK_SDL2)
        if (p.sdlID > 0)
        {
            SDL_CloseAudioDevice(p.sdlID);
            p.sdlID = 0;
        }
#elif defined(FTK_SDL3)
        if (p.sdlStream)
        {
            SDL_DestroyAudioStream(p.sdlStream);
            p.sdlStream = nullptr;
        }
#endif // FTK_SDL2

        --objectCount;
    }

    std::shared_ptr<Player> Player::create(
        const std::shared_ptr<ftk::Context>& context,
        const std::shared_ptr<Timeline>& timeline,
        const PlayerOptions& playerOptions)
    {
        auto out = std::shared_ptr<Player>(new Player);
        out->_init(context, timeline, playerOptions);
        return out;
    }

    std::shared_ptr<ftk::Context> Player::getContext() const
    {
        return _p->timeline->getContext();
    }
        
    const std::shared_ptr<Timeline>& Player::getTimeline() const
    {
        return _p->timeline;
    }

    const ftk::Path& Player::getPath() const
    {
        return _p->timeline->getPath();
    }

    const ftk::Path& Player::getAudioPath() const
    {
        return _p->timeline->getAudioPath();
    }

    const PlayerOptions& Player::getPlayerOptions() const
    {
        return _p->playerOptions;
    }

    const Options& Player::getOptions() const
    {
        return _p->timeline->getOptions();
    }

    const OTIO_NS::TimeRange& Player::getTimeRange() const
    {
        return _p->timeRange;
    }

    OTIO_NS::RationalTime Player::getDuration() const
    {
        return _p->timeRange.duration();
    }

    const IOInfo& Player::getIOInfo() const
    {
        // From the timeline, so that the video information follows the media
        // reference key.
        //
        // The audio information has no need to follow it. The timeline hands
        // the readers its own audio format to convert to, so every media
        // reference is read in the format found when the timeline was read,
        // whichever one is active. That format is kept as sourceAudioInfo,
        // which is what the audio thread reads.
        return _p->timeline->getIOInfo();
    }

    double Player::getDefaultSpeed() const
    {
        return _p->timeRange.duration().rate();
    }

    double Player::getSpeed() const
    {
        return _p->speed->get();
    }

    std::shared_ptr<ftk::IObservable<double> > Player::observeSpeed() const
    {
        return _p->speed;
    }

    void Player::setSpeed(double value)
    {
        FTK_P();
        if (p.speed->setIfChanged(value))
        {
            const double actualSpeed = value * p.speedMult->get();
            p.actualSpeed->setIfChanged(actualSpeed);
            {
                std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                p.audioMutex.state.speed = actualSpeed;
                p.audioReset(p.currentTime->get());
            }
            p.playbackReset(p.currentTime->get());
        }
    }

    double Player::getSpeedMult() const
    {
        return _p->speedMult->get();
    }

    std::shared_ptr<ftk::IObservable<double> > Player::observeSpeedMult() const
    {
        return _p->speedMult;
    }

    void Player::setSpeedMult(double value)
    {
        FTK_P();
        p.accelerate = 0;
        _setSpeedMult(value);
    }

    double Player::getActualSpeed() const
    {
        return _p->actualSpeed->get();
    }

    std::shared_ptr<ftk::IObservable<double> > Player::observeActualSpeed() const
    {
        return _p->actualSpeed;
    }

    size_t Player::getDroppedFrames() const
    {
        return _p->droppedFrames->get();
    }

    std::shared_ptr<ftk::IObservable<size_t> > Player::observeDroppedFrames() const
    {
        return _p->droppedFrames;
    }

    Playback Player::getPlayback() const
    {
        return _p->playback->get();
    }

    std::shared_ptr<ftk::IObservable<Playback> > Player::observePlayback() const
    {
        return _p->playback;
    }

    void Player::setPlayback(Playback value)
    {
        FTK_P();

        // Update the frame for loop modes.
        switch (p.loop->get())
        {
        case Loop::Once:
            switch (value)
            {
            case Playback::Forward:
                if (p.currentTime->get() == p.inOutRange->get().end_time_inclusive())
                {
                    seek(p.inOutRange->get().start_time());
                }
                break;
            case Playback::Reverse:
                if (p.currentTime->get() == p.inOutRange->get().start_time())
                {
                    seek(p.inOutRange->get().end_time_inclusive());
                }
                break;
            default: break;
            }
            break;
        case Loop::PingPong:
            switch (value)
            {
            case Playback::Forward:
                if (p.currentTime->get() == p.inOutRange->get().end_time_inclusive())
                {
                    value = Playback::Reverse;
                }
                break;
            case Playback::Reverse:
                if (p.currentTime->get() == p.inOutRange->get().start_time())
                {
                    value = Playback::Forward;
                }
                break;
            default: break;
            }
            break;
        default: break;
        }

        if (value != Playback::Stop && value == p.playback->get())
        {
            p.accelerate = std::min(p.accelerate + 1, 6);
            _setSpeedMult(pow(2, p.accelerate));
        }
        else if (value != Playback::Stop && p.accelerate > 0)
        {
            --p.accelerate;
            _setSpeedMult(pow(2, p.accelerate));
        }
        else if (p.playback->setIfChanged(value))
        {
            p.droppedFrames->setIfChanged(0);
            p.droppedFramesReset = true;
            if (value != Playback::Stop)
            {
                p.toggle = value;
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    p.mutex.state.playback = value;
                    p.mutex.state.currentTime = p.currentTime->get();
                    p.mutex.clearRequests = true;
                    p.mutex.cacheDir = Playback::Forward == value ?
                        CacheDir::Forward :
                        CacheDir::Reverse;
                }
                {
                    std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                    p.audioMutex.state.playback = value;
                    p.audioReset(p.currentTime->get());
                }
                p.playbackReset(p.currentTime->get());
            }
            else
            {
                if (p.accelerate > 0)
                {
                    p.accelerate = 0;
                    _setSpeedMult(1.0);
                }
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    p.mutex.state.playback = value;
                    p.mutex.clearRequests = true;
                }
                {
                    std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                    p.audioMutex.state.playback = value;
                }
            }
        }
    }

    void Player::togglePlayback()
    {
        FTK_P();
        if (Playback::Stop == p.playback->get())
        {
            setPlayback(p.toggle);
        }
        else
        {
            stop();
        }
    }

    bool Player::isStopped() const
    {
        return Playback::Stop == _p->playback->get();
    }

    void Player::stop()
    {
        setPlayback(Playback::Stop);
    }

    void Player::forward()
    {
        setPlayback(Playback::Forward);
    }

    void Player::reverse()
    {
        setPlayback(Playback::Reverse);
    }

    Loop Player::getLoop() const
    {
        return _p->loop->get();
    }

    std::shared_ptr<ftk::IObservable<Loop> > Player::observeLoop() const
    {
        return _p->loop;
    }

    void Player::setLoop(Loop value)
    {
        _p->loop->setIfChanged(value);
    }

    const OTIO_NS::RationalTime& Player::getCurrentTime() const
    {
        return _p->currentTime->get();
    }

    std::shared_ptr<ftk::IObservable<OTIO_NS::RationalTime> > Player::observeCurrentTime() const
    {
        return _p->currentTime;
    }

    std::shared_ptr<ftk::IObservable<OTIO_NS::RationalTime> > Player::observeSeek() const
    {
        return _p->seek;
    }

    void Player::seek(const OTIO_NS::RationalTime& time)
    {
        FTK_P();

        // Loop the time.
        OTIO_NS::RationalTime tmp = time.rescaled_to(p.timeRange.duration()).floor();
        if (tmp > p.timeRange.end_time_inclusive())
        {
            tmp = p.timeRange.start_time();
        }
        else if (tmp < p.timeRange.start_time())
        {
            tmp = p.timeRange.end_time_inclusive();
        }

        if (p.currentTime->setIfChanged(tmp))
        {
            //std::cout << "seek: " << tmp << std::endl;
            p.seek->setAlways(tmp);
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                p.mutex.state.currentTime = tmp;
                p.mutex.clearRequests = true;
            }
            p.resetPlaybackTime(tmp);
        }
    }

    void Player::timeAction(TimeAction time)
    {
        FTK_P();
        const auto& currentTime = p.currentTime->get();
        switch (time)
        {
        case TimeAction::Start:
            setPlayback(Playback::Stop);
            seek(p.inOutRange->get().start_time());
            break;
        case TimeAction::End:
            setPlayback(Playback::Stop);
            seek(p.inOutRange->get().end_time_inclusive());
            break;
        case TimeAction::FramePrev:
            setPlayback(Playback::Stop);
            seek(currentTime - OTIO_NS::RationalTime(1, p.timeRange.duration().rate()));
            break;
        case TimeAction::FramePrevX10:
            setPlayback(Playback::Stop);
            seek(currentTime - OTIO_NS::RationalTime(10, p.timeRange.duration().rate()));
            break;
        case TimeAction::FramePrevX100:
            setPlayback(Playback::Stop);
            seek(currentTime - OTIO_NS::RationalTime(100, p.timeRange.duration().rate()));
            break;
        case TimeAction::FrameNext:
            setPlayback(Playback::Stop);
            seek(currentTime + OTIO_NS::RationalTime(1, p.timeRange.duration().rate()));
            break;
        case TimeAction::FrameNextX10:
            setPlayback(Playback::Stop);
            seek(currentTime + OTIO_NS::RationalTime(10, p.timeRange.duration().rate()));
            break;
        case TimeAction::FrameNextX100:
            setPlayback(Playback::Stop);
            seek(currentTime + OTIO_NS::RationalTime(100, p.timeRange.duration().rate()));
            break;
        case TimeAction::JumpBack1s:
            seek(currentTime - OTIO_NS::RationalTime(1.0, 1.0));
            break;
        case TimeAction::JumpBack10s:
            seek(currentTime - OTIO_NS::RationalTime(10.0, 1.0));
            break;
        case TimeAction::JumpForward1s:
            seek(currentTime + OTIO_NS::RationalTime(1.0, 1.0));
            break;
        case TimeAction::JumpForward10s:
            seek(currentTime + OTIO_NS::RationalTime(10.0, 1.0));
            break;
        default: break;
        }
    }

    void Player::gotoStart()
    {
        timeAction(TimeAction::Start);
    }

    void Player::gotoEnd()
    {
        timeAction(TimeAction::End);
    }

    void Player::framePrev()
    {
        timeAction(TimeAction::FramePrev);
    }

    void Player::frameNext()
    {
        timeAction(TimeAction::FrameNext);
    }

    const OTIO_NS::TimeRange& Player::getInOutRange() const
    {
        return _p->inOutRange->get();
    }

    std::shared_ptr<ftk::IObservable<OTIO_NS::TimeRange> > Player::observeInOutRange() const
    {
        return _p->inOutRange;
    }

    void Player::setInOutRange(const OTIO_NS::TimeRange& value)
    {
        FTK_P();
        OTIO_NS::TimeRange tmp(
            value.start_time().rescaled_to(p.timeRange.duration().rate()).floor(),
            value.duration().rescaled_to(p.timeRange.duration().rate()).ceil());
        tmp = p.timeRange.clamped(tmp);
        if (p.inOutRange->setIfChanged(tmp))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.inOutRange = tmp;
            p.mutex.clearRequests = true;
        }
    }

    void Player::setInPoint()
    {
        FTK_P();
        setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time(
            p.currentTime->get(),
            p.inOutRange->get().end_time_exclusive()));
    }

    void Player::resetInPoint()
    {
        FTK_P();
        setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time(
            p.timeRange.start_time(),
            p.inOutRange->get().end_time_exclusive()));
    }

    void Player::setOutPoint()
    {
        FTK_P();
        setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
            p.inOutRange->get().start_time(),
            p.currentTime->get()));
    }

    void Player::resetOutPoint()
    {
        FTK_P();
        setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
            p.inOutRange->get().start_time(),
            p.timeRange.end_time_inclusive()));
    }

    const std::vector<std::shared_ptr<Timeline> >& Player::getCompare() const
    {
        return _p->compare->get();
    }

    std::shared_ptr<ftk::IObservableList<std::shared_ptr<Timeline> > > Player::observeCompare() const
    {
        return _p->compare;
    }

    void Player::setCompare(const std::vector<std::shared_ptr<Timeline> >& value)
    {
        FTK_P();
        if (p.compare->setIfChanged(value))
        {
            // Bring the comparison timelines onto the media reference key that
            // is already in use.
            for (const auto& timeline : value)
            {
                timeline->setMediaReferenceKey(p.mediaReferenceKey->get());
            }
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.compare = value;
            p.mutex.clearRequests = true;
            p.mutex.clearCache = true;
        }
    }

    CompareTime Player::getCompareTime() const
    {
        return _p->compareTime->get();
    }

    std::shared_ptr<ftk::IObservable<CompareTime> > Player::observeCompareTime() const
    {
        return _p->compareTime;
    }

    void Player::setCompareTime(CompareTime value)
    {
        FTK_P();
        if (p.compareTime->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.compareTime = value;
            p.mutex.clearRequests = true;
            p.mutex.clearCache = true;
        }
    }

    const IOOptions& Player::getIOOptions() const
    {
        return _p->ioOptions->get();
    }

    std::shared_ptr<ftk::IObservable<IOOptions> > Player::observeIOOptions() const
    {
        return _p->ioOptions;
    }

    void Player::setIOOptions(const IOOptions& value)
    {
        FTK_P();
        if (p.ioOptions->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.ioOptions = value;
            p.mutex.clearRequests = true;
            p.mutex.clearCache = true;
        }
    }

    const std::string& Player::getMediaReferenceKey() const
    {
        return _p->mediaReferenceKey->get();
    }

    std::shared_ptr<ftk::IObservable<std::string> > Player::observeMediaReferenceKey() const
    {
        return _p->mediaReferenceKey;
    }

    void Player::setMediaReferenceKey(const std::string& value)
    {
        FTK_P();
        if (value != p.mediaReferenceKey->get())
        {
            p.timeline->setMediaReferenceKey(value);
            for (const auto& timeline : p.compare->get())
            {
                timeline->setMediaReferenceKey(value);
            }
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                p.mutex.clearRequests = true;
                p.mutex.clearCache = true;
            }

            // Notified last, so that an observer asking the timeline what it
            // is reading sees the new media reference and not the old one.
            p.mediaReferenceKey->setIfChanged(value);
        }
    }

    std::vector<std::string> Player::getMediaReferenceKeys() const
    {
        FTK_P();
        std::set<std::string> keys;
        for (const auto& key : p.timeline->getMediaReferenceKeys())
        {
            keys.insert(key);
        }
        for (const auto& timeline : p.compare->get())
        {
            for (const auto& key : timeline->getMediaReferenceKeys())
            {
                keys.insert(key);
            }
        }
        return std::vector<std::string>(keys.begin(), keys.end());
    }

    int Player::getVideoLayer() const
    {
        return _p->videoLayer->get();
    }

    std::shared_ptr<ftk::IObservable<int> > Player::observeVideoLayer() const
    {
        return _p->videoLayer;
    }

    void Player::setVideoLayer(int value)
    {
        FTK_P();
        if (p.videoLayer->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.videoLayer = value;
            p.mutex.clearRequests = true;
            p.mutex.clearCache = true;
        }
    }

    const std::vector<int>& Player::getCompareVideoLayers() const
    {
        return _p->compareVideoLayers->get();
    }

    std::shared_ptr<ftk::IObservableList<int> > Player::observeCompareVideoLayers() const
    {
        return _p->compareVideoLayers;
    }

    void Player::setCompareVideoLayers(const std::vector<int>& value)
    {
        FTK_P();
        if (p.compareVideoLayers->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.compareVideoLayers = value;
            p.mutex.clearRequests = true;
            p.mutex.clearCache = true;
        }
    }

    const std::vector<VideoFrame>& Player::getCurrentVideo() const
    {
        return _p->currentVideoFrame->get();
    }

    std::shared_ptr<ftk::IObservableList<VideoFrame> > Player::observeCurrentVideo() const
    {
        return _p->currentVideoFrame;
    }

    const PlayerCacheOptions& Player::getCacheOptions() const
    {
        return _p->cacheOptions->get();
    }

    std::shared_ptr<ftk::IObservable<PlayerCacheOptions> > Player::observeCacheOptions() const
    {
        return _p->cacheOptions;
    }

    void Player::setCacheOptions(const PlayerCacheOptions& value)
    {
        FTK_P();
        if (p.cacheOptions->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.cacheOptions = value;
        }
    }

    std::shared_ptr<ftk::IObservable<PlayerCacheInfo> > Player::observeCacheInfo() const
    {
        return _p->cacheInfo;
    }

    void Player::clearCache()
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        p.mutex.clearRequests = true;
        p.mutex.clearCache = true;
    }

    void Player::_setSpeedMult(double value)
    {
        FTK_P();
        if (p.speedMult->setIfChanged(value))
        {
            const double actualSpeed = p.speed->get() * value;
            p.actualSpeed->setIfChanged(actualSpeed);
            {
                std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                p.audioMutex.state.speed = actualSpeed;
                p.audioReset(p.currentTime->get());
            }
            p.playbackReset(p.currentTime->get());
        }
    }

    void Player::_tick()
    {
        FTK_P();

        // Calculate the current time.
        const double timelineSpeed = p.timeRange.duration().rate();
        const auto playback = p.playback->get();
        if (playback != Playback::Stop && timelineSpeed > 0.0)
        {
            OTIO_NS::RationalTime start = invalidTime;
            double t = 0.0;
            if (p.hasAudio())
            {
                std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                start = p.audioMutex.start;
                t = OTIO_NS::RationalTime(p.audioMutex.frame, p.sourceAudioInfo.sampleRate).rescaled_to(1.0).value();
            }
            else
            {
                std::chrono::steady_clock::time_point playbackTimer;
                {
                    std::unique_lock<std::mutex> lock(p.noAudio.mutex);
                    start = p.noAudio.start;
                    playbackTimer = p.noAudio.playbackTimer;
                }
                const auto now = std::chrono::steady_clock::now();
                const std::chrono::duration<double> diff = now - playbackTimer;
                t = diff.count() * (p.speed->get() * p.speedMult->get()) / timelineSpeed;
            }
            if (Playback::Reverse == playback)
            {
                t = -t;
            }
            bool looped = false;
            const OTIO_NS::RationalTime currentTime = p.loopPlayback(
                start +
                OTIO_NS::RationalTime(t, 1.0).rescaled_to(timelineSpeed).floor(),
                looped);
            //const double currentTimeDiff = abs(currentTime.value() - p.currentTime->get().value());
            if (p.currentTime->setIfChanged(currentTime))
            {
                //std::cout << "current time: " << p.currentTime->get() << " / " << currentTimeDiff << std::endl;
                if (looped)
                {
                    p.seek->setAlways(currentTime);
                }
            }
        }

        // Sync with the thread.
        std::vector<VideoFrame> currentVideoFrame;
        std::vector<AudioFrame> currentAudioFrame;
        PlayerCacheInfo cacheInfo;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.state.currentTime = p.currentTime->get();
            currentVideoFrame = p.mutex.currentVideoFrame;
            currentAudioFrame = p.mutex.currentAudioFrame;
            cacheInfo = p.mutex.cacheInfo;
        }
        p.currentVideoFrame->setIfChanged(currentVideoFrame);
        p.currentAudioFrame->setIfChanged(currentAudioFrame);
        p.cacheInfo->setIfChanged(cacheInfo);

        if (playback != Playback::Stop && timelineSpeed > 0.0)
        {
            p.droppedFramesTick(
                !currentVideoFrame.empty() ? currentVideoFrame[0].time : invalidTime,
                playback,
                timelineSpeed);
        }
    }

    void Player::_thread()
    {
        FTK_P();
        p.thread.cacheTimer = std::chrono::steady_clock::now();
        p.thread.logTimer = std::chrono::steady_clock::now();
        while (p.running)
        {
            const auto t0 = std::chrono::steady_clock::now();

            // Get mutex protected values.
            Private::PlaybackState state;
            bool clearRequests = false;
            bool clearCache = false;
            CacheDir cacheDir = CacheDir::First;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                state = p.mutex.state;
                clearRequests = p.mutex.clearRequests;
                p.mutex.clearRequests = false;
                clearCache = p.mutex.clearCache;
                p.mutex.clearCache = false;
                cacheDir = p.mutex.cacheDir;
            }
            p.thread.state = state;
            p.thread.cacheDir = cacheDir;

            // Clear requests.
            if (clearRequests)
            {
                p.clearRequests();
            }

            // Clear the cache.
            if (clearCache)
            {
                p.clearCache();
            }

            // Update the cache.
            p.cacheUpdate();

            // Update the current video frame.
            if (p.hasVideo())
            {
                const auto i = p.thread.videoCache.find(p.thread.state.currentTime);
                if (i != p.thread.videoCache.end())
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    p.mutex.currentVideoFrame = i->second;
                }
                else if (p.thread.state.playback != Playback::Stop)
                {
                    if (!p.timeRange.contains(p.thread.state.currentTime))
                    {
                        std::unique_lock<std::mutex> lock(p.mutex.mutex);
                        p.mutex.currentVideoFrame.clear();
                    }
                    const auto now = std::chrono::steady_clock::now();
                    if (now > p.audioMutex.state.muteTimeout)
                    {
                        {
                            std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                            p.audioMutex.state.muteTimeout = now + p.playerOptions.muteTimeout;
                            p.audioReset(p.currentTime->get());
                        }
                        p.playbackReset(p.currentTime->get());
                    }
                }
                else
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    if (!p.timeRange.contains(p.thread.state.currentTime))
                    {
                        p.mutex.currentVideoFrame.clear();
                    }
                }
            }

            // Update the current audio frames.
            if (p.sourceAudioInfo.isValid())
            {
                std::vector<AudioFrame> audioFrameList;
                {
                    const int64_t seconds =
                        p.thread.state.currentTime.rescaled_to(1.0).value() -
                        p.thread.state.audioOffset;
                    std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                    for (int64_t s : { seconds - 1, seconds, seconds + 1 })
                    {
                        const auto i = p.audioMutex.cache.find(s);
                        if (i != p.audioMutex.cache.end())
                        {
                            audioFrameList.push_back(i->second);
                        }
                    }
                }
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    p.mutex.currentAudioFrame = audioFrameList;
                }
            }

            // Logging.
            auto t1 = std::chrono::steady_clock::now();
            const std::chrono::duration<double> diff = t1 - p.thread.logTimer;
            if (diff.count() > 10.0)
            {
                p.thread.logTimer = t1;
                if (auto context = getContext())
                {
                    p.log();
                }
                t1 = std::chrono::steady_clock::now();
            }

            // Sleep for a bit.
            ftk::sleep(p.playerOptions.sleepTimeout, t0, t1);
        }

        // Finished.
        p.clearRequests();
    }

    size_t Player::getObjectCount()
    {
        return objectCount;
    }
}
