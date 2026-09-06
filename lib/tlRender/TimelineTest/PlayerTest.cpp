// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/PlayerTest.h>

#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Util.h>

#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Time.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/timeline.h>

#include <sstream>

namespace tl
{
    namespace timeline_tests
    {
        namespace
        {
            // How long to let the playback thread run in each state. This is
            // spent waiting rather than working, and it is multiplied by the
            // number of timelines, loop modes, and playback states covered
            // below, so it dominates the time this test takes. Keep it short.
            const std::chrono::duration<float> playbackTime(0.25F);

            void waitForPlayback()
            {
                const auto t = std::chrono::steady_clock::now();
                std::chrono::duration<float> diff;
                do
                {
                    ftk::sleep(std::chrono::milliseconds(10));
                    diff = std::chrono::steady_clock::now() - t;
                } while (diff < playbackTime);
            }
        }

        PlayerTest::PlayerTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::PlayerTest")
        {}

        std::shared_ptr<PlayerTest> PlayerTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<PlayerTest>(new PlayerTest(context));
        }

        void PlayerTest::run()
        {
            _enums();
            _player();
            _seqAndAudio();
            _compare();
        }

        void PlayerTest::_enums()
        {
            FTK_TEST_ENUM(Playback);
            FTK_TEST_ENUM(Loop);
            FTK_TEST_ENUM(TimeAction);
        }

        void PlayerTest::_player()
        {
            // Test timeline players.
            const std::vector<ftk::Path> paths =
            {
                ftk::Path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg"),
#if defined(TLRENDER_FFMPEG_PLUGIN)
                ftk::Path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MovieAndSeq.otio"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "TransitionGap.otio"),
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_FFMPEG_PLUGIN)
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClip.otioz"),
#endif // TLRENDER_FFMPEG_PLUGIN
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz")
            };
            for (const auto& path : paths)
            {
                try
                {
                    _print(ftk::Format("Timeline: {0}").arg(path.get()));
                    // Opened the way an application opens: gathering a frame
                    // into the sequence it belongs to is getPaths()' job, not
                    // the timeline's.
                    ftk::DirListOptions dirListOptions;
                    dirListOptions.seqExts = getExts(
                        _context, static_cast<int>(FileType::Seq));
                    const auto opened = getPaths(_context, path, dirListOptions);
                    auto timeline = Timeline::create(
                        _context, opened.empty() ? path : opened.front());
                    auto player = Player::create(_context, timeline);
                    FTK_CHECK(player->getTimeline());
                    _player(player);
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                }
            }
        }

        void PlayerTest::_player(const std::shared_ptr<Player>& player)
        {
            const ftk::Path& path = player->getPath();
            const ftk::Path& audioPath = player->getAudioPath();
            const Options options = player->getOptions();
            const OTIO_NS::TimeRange& timeRange = player->getTimeRange();
            const IOInfo& ioInfo = player->getIOInfo();
            const double defaultSpeed = player->getDefaultSpeed();
            double speed = player->getSpeed();
            _print(ftk::Format("Path: {0}").arg(path.get()));
            _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            _print(ftk::Format("Time range: {0}").arg(timeRange));
            if (!ioInfo.video.empty())
            {
                _print(ftk::Format("Video: {0}").arg(ioInfo.video.size()));
            }
            if (ioInfo.audio.isValid())
            {
                _print(ftk::Format("Audio: {0} {1} {2}").
                    arg(ioInfo.audio.channelCount).
                    arg(ioInfo.audio.type).
                    arg(ioInfo.audio.sampleRate));
            }
            _print(ftk::Format("Default speed: {0}").arg(defaultSpeed));
            _print(ftk::Format("Speed: {0}").arg(speed));

            // Test the playback speed.
            auto speedObserver = ftk::Observer<double>::create(
                player->observeSpeed(),
                [&speed](double value)
                {
                    speed = value;
                });
            const double doubleSpeed = defaultSpeed * 2.0;
            player->setSpeed(doubleSpeed);
            FTK_CHECK(doubleSpeed == speed);
            player->setSpeed(defaultSpeed);

            // Test the playback mode.
            Playback playback = Playback::Stop;
            auto playbackObserver = ftk::Observer<Playback>::create(
                player->observePlayback(),
                [&playback](Playback value)
                {
                    playback = value;
                });
            player->setPlayback(Playback::Forward);
            FTK_CHECK(Playback::Forward == player->getPlayback());
            FTK_CHECK(Playback::Forward == playback);

            // Test the playback loop mode.
            Loop loop = Loop::Loop;
            auto loopObserver = ftk::Observer<Loop>::create(
                player->observeLoop(),
                [&loop](Loop value)
                {
                    loop = value;
                });
            player->setLoop(Loop::Once);
            FTK_CHECK(Loop::Once == player->getLoop());
            FTK_CHECK(Loop::Once == loop);

            // Test the current time.
            player->setPlayback(Playback::Stop);
            OTIO_NS::RationalTime currentTime;
            auto currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                player->observeCurrentTime(),
                [&currentTime](const OTIO_NS::RationalTime& value)
                {
                    currentTime = value;
                });
            player->seek(timeRange.start_time());
            FTK_CHECK(timeRange.start_time() == player->getCurrentTime());
            FTK_CHECK(timeRange.start_time() == currentTime);
            const double rate = timeRange.duration().rate();
            player->seek(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate));
            FTK_CHECK(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate) ==
                currentTime);
            player->gotoEnd();
            FTK_CHECK(timeRange.end_time_inclusive() == currentTime);
            player->gotoStart();
            FTK_CHECK(timeRange.start_time() == currentTime);
            player->frameNext();
            FTK_CHECK(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate) ==
                currentTime);
            player->timeAction(TimeAction::FrameNextX10);
            player->timeAction(TimeAction::FrameNextX100);
            player->framePrev();
            player->timeAction(TimeAction::FramePrevX10);
            player->timeAction(TimeAction::FramePrevX100);
            player->timeAction(TimeAction::JumpForward1s);
            player->timeAction(TimeAction::JumpForward10s);
            player->timeAction(TimeAction::JumpBack1s);
            player->timeAction(TimeAction::JumpBack10s);

            // Test the in/out points.
            OTIO_NS::TimeRange inOutRange;
            auto inOutRangeObserver = ftk::Observer<OTIO_NS::TimeRange>::create(
                player->observeInOutRange(),
                [&inOutRange](const OTIO_NS::TimeRange& value)
                {
                    inOutRange = value;
                });
            player->setInOutRange(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)));
            FTK_CHECK(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)) == player->getInOutRange());
            FTK_CHECK(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)) == inOutRange);
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate));
            player->setInPoint();
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(10.0, rate));
            player->setOutPoint();
            FTK_CHECK(OTIO_NS::TimeRange(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate),
                OTIO_NS::RationalTime(10.0, rate)) == inOutRange);
            player->resetInPoint();
            player->resetOutPoint();
            FTK_CHECK(OTIO_NS::TimeRange(timeRange.start_time(), timeRange.duration()) == inOutRange);

            // Test the comparison time options.
            std::vector<OTIO_NS::TimeRange> compareInOutRanges;
            auto compareInOutRangesObserver =
                ftk::Observer<std::vector<OTIO_NS::TimeRange> >::create(
                    player->observeCompareInOutRanges(),
                    [&compareInOutRanges](const std::vector<OTIO_NS::TimeRange>& value)
                    {
                        compareInOutRanges = value;
                    });
            const std::vector<OTIO_NS::TimeRange> compareInOutRanges2 =
            {
                OTIO_NS::TimeRange(
                    timeRange.start_time(),
                    OTIO_NS::RationalTime(10.0, rate))
            };
            player->setCompareInOutRanges(compareInOutRanges2);
            FTK_ASSERT(compareInOutRanges2 == player->getCompareInOutRanges());
            FTK_ASSERT(compareInOutRanges2 == compareInOutRanges);

            CompareTimeOptions compareTimeOptions;
            auto compareTimeOptionsObserver =
                ftk::Observer<CompareTimeOptions>::create(
                    player->observeCompareTimeOptions(),
                    [&compareTimeOptions](const CompareTimeOptions& value)
                    {
                        compareTimeOptions = value;
                    });
            CompareTimeOptions compareTimeOptions2;
            compareTimeOptions2.alignInPoints = true;
            compareTimeOptions2.frameOffset = 2;
            player->setCompareTimeOptions(compareTimeOptions2);
            FTK_ASSERT(compareTimeOptions2 == player->getCompareTimeOptions());
            FTK_ASSERT(compareTimeOptions2 == compareTimeOptions);
            player->setCompareInOutRanges({});
            player->setCompareTimeOptions({});

            // Test the I/O options.
            IOOptions ioOptions;
            auto ioOptionsObserver = ftk::Observer<IOOptions>::create(
                player->observeIOOptions(),
                [&ioOptions](const IOOptions& value)
                {
                    ioOptions = value;
                });
            IOOptions ioOptions2;
            ioOptions2["Layer"] = "1";
            player->setIOOptions(ioOptions2);
            FTK_CHECK(ioOptions2 == player->getIOOptions());
            FTK_CHECK(ioOptions2 == ioOptions);
            player->setIOOptions({});

            // Test the video layers.
            int videoLayer = 0;
            std::vector<int> compareVideoLayers;
            auto videoLayerObserver = ftk::Observer<int>::create(
                player->observeVideoLayer(),
                [&videoLayer](int value)
                {
                    videoLayer = value;
                });
            auto compareVideoLayersObserver = ftk::ListObserver<int>::create(
                player->observeCompareVideoLayers(),
                [&compareVideoLayers](const std::vector<int>& value)
                {
                    compareVideoLayers = value;
                });
            int videoLayer2 = 1;
            player->setVideoLayer(videoLayer2);
            FTK_CHECK(videoLayer2 == player->getVideoLayer());
            FTK_CHECK(videoLayer2 == videoLayer);
            std::vector<int> compareVideoLayers2 = { 2, 3 };
            player->setCompareVideoLayers(compareVideoLayers2);
            FTK_CHECK(compareVideoLayers2 == player->getCompareVideoLayers());
            FTK_CHECK(compareVideoLayers2 == compareVideoLayers);
            player->setVideoLayer(0);
            player->setCompareVideoLayers({});

            // Test audio.
            float volume = 1.F;
            auto volumeObserver = ftk::Observer<float>::create(
                player->observeVolume(),
                [&volume](float value)
                {
                    volume = value;
                });
            player->setVolume(.5F);
            FTK_CHECK(.5F == player->getVolume());
            FTK_CHECK(.5F == volume);
            player->setVolume(1.F);

            bool mute = false;
            auto muteObserver = ftk::Observer<bool>::create(
                player->observeMute(),
                [&mute](bool value)
                {
                    mute = value;
                });
            player->setMute(true);
            FTK_CHECK(player->isMuted());
            FTK_CHECK(mute);
            player->setMute(false);

            std::vector<bool> channelMute = { false, false };
            auto channelMuteObserver = ftk::ListObserver<bool>::create(
                player->observeChannelMute(),
                [&channelMute](const std::vector<bool>& value)
                {
                    channelMute = value;
                });
            player->setChannelMute({ true, true });
            FTK_CHECK(player->getChannelMute() == std::vector<bool>({ true, true }));
            FTK_CHECK(channelMute[0]);
            FTK_CHECK(channelMute[1]);
            player->setChannelMute({ false, false });

            double audioOffset = 0.0;
            auto audioOffsetObserver = ftk::Observer<double>::create(
                player->observeAudioOffset(),
                [&audioOffset](double value)
                {
                    audioOffset = value;
                });
            player->setAudioOffset(0.5);
            FTK_CHECK(0.5 == player->getAudioOffset());
            FTK_CHECK(0.5 == audioOffset);
            player->setAudioOffset(0.0);
            
            // Test frames.
            {
                PlayerCacheOptions cacheOptions;
                auto cacheOptionsObserver = ftk::Observer<PlayerCacheOptions>::create(
                    player->observeCacheOptions(),
                    [&cacheOptions](const PlayerCacheOptions& value)
                    {
                        cacheOptions = value;
                    });
                cacheOptions.videoGB = 1.F;
                player->setCacheOptions(cacheOptions);
                FTK_CHECK(cacheOptions == player->getCacheOptions());

                auto currentVideoObserver = ftk::ListObserver<VideoFrame>::create(
                    player->observeCurrentVideo(),
                    [this](const std::vector<VideoFrame>& value)
                    {
                        std::stringstream ss;
                        ss << "Video time: ";
                        if (!value.empty())
                        {
                            ss << value.front().time;
                        }
                        _print(ss.str());
                    });
                auto currentAudioObserver = ftk::ListObserver<AudioFrame>::create(
                    player->observeCurrentAudio(),
                    [this](const std::vector<AudioFrame>& value)
                    {
                        for (const auto& i : value)
                        {
                            std::stringstream ss;
                            ss << "Audio time: " << i.seconds;
                            _print(ss.str());
                        }
                    });
                auto cacheInfoObserver = ftk::Observer<PlayerCacheInfo>::create(
                    player->observeCacheInfo(),
                    [this](const PlayerCacheInfo& value)
                    {
                        {
                            std::stringstream ss;
                            ss << "Video/audio cached frames: " << value.video.size() << "/" << value.audio.size();
                            _print(ss.str());
                        }
                    });

                for (const auto& loop : getLoopEnums())
                {
                    player->seek(timeRange.start_time());
                    player->setLoop(loop);
                    player->setPlayback(Playback::Forward);
                    waitForPlayback();

                    player->seek(timeRange.end_time_inclusive());
                    waitForPlayback();

                    player->seek(timeRange.end_time_inclusive());
                    player->setPlayback(Playback::Reverse);
                    waitForPlayback();

                    player->seek(timeRange.start_time());
                    player->setSpeed(doubleSpeed);
                    waitForPlayback();
                    player->setSpeed(defaultSpeed);
                }
                player->setPlayback(Playback::Stop);
                player->clearCache();
            }
        }

        //! An image sequence with a separate audio file, which gives a player
        //! with both without depending on a codec: the frames are JPEG and
        //! the audio is PCM.
        void PlayerTest::_seqAndAudio()
        {
            try
            {
                auto timeline = Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg"),
                    ftk::Path(TLRENDER_SAMPLE_DATA, "AudioToneStereo.wav"));
                auto player = Player::create(_context, timeline);
                const IOInfo& ioInfo = player->getIOInfo();
                FTK_CHECK(!ioInfo.video.empty());
                if (ioInfo.audio.isValid())
                {
                    // The audio is in the directory above the sequence, so
                    // this only holds while the reference to it keeps the
                    // directory it was given.
                    FTK_CHECK(ioInfo.audio.channelCount > 0);
                    FTK_CHECK(ioInfo.audio.sampleRate > 0);
                    _print(ftk::Format("Sequence and audio: {0} {1}Hz, {2}").
                        arg(ioInfo.audio.channelCount).
                        arg(ioInfo.audio.sampleRate).
                        arg(player->getDuration()));
                }
                else
                {
                    // A build with no reader for the audio file, which the
                    // minimal configuration is. The rest of this still
                    // covers the player, which is what it is here for.
                    _print("Sequence and audio: no audio reader in this build");
                }

                // The playback the tool bar drives, rather than setPlayback.
                FTK_CHECK(player->isStopped());
                player->forward();
                FTK_CHECK(Playback::Forward == player->getPlayback());
                FTK_CHECK(!player->isStopped());
                waitForPlayback();
                player->togglePlayback();
                FTK_CHECK(player->isStopped());
                player->togglePlayback();
                FTK_CHECK(Playback::Forward == player->getPlayback());
                player->reverse();
                FTK_CHECK(Playback::Reverse == player->getPlayback());
                waitForPlayback();
                player->stop();
                FTK_CHECK(player->isStopped());

                // The speed multiplier, which is separate from the speed.
                double speedMult = 0.0;
                auto speedMultObserver = ftk::Observer<double>::create(
                    player->observeSpeedMult(),
                    [&speedMult](double value) { speedMult = value; });
                player->setSpeedMult(2.0);
                FTK_CHECK(2.0 == player->getSpeedMult());
                FTK_CHECK(2.0 == speedMult);
                FTK_CHECK(player->getActualSpeed() == player->getSpeed() * 2.0);
                auto actualSpeedObserver = ftk::Observer<double>::create(
                    player->observeActualSpeed(),
                    [](double) {});
                player->setSpeedMult(1.0);

                // Dropped frames, which only playback produces.
                auto droppedObserver = ftk::Observer<size_t>::create(
                    player->observeDroppedFrames(),
                    [](size_t) {});
                player->forward();
                waitForPlayback();
                player->stop();
                _print(ftk::Format("Dropped frames: {0}").
                    arg(player->getDroppedFrames()));

                // The media references of the clip, and the seek and audio
                // device observers, which nothing else here reaches.
                auto seekObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                    player->observeSeek(),
                    [](const OTIO_NS::RationalTime&) {});
                auto audioDeviceObserver = ftk::Observer<AudioDeviceID>::create(
                    player->observeAudioDevice(),
                    [](const AudioDeviceID&) {});
                auto keyObserver = ftk::Observer<std::string>::create(
                    player->observeMediaReferenceKey(),
                    [](const std::string&) {});
                const auto keys = player->getMediaReferenceKeys();
                _print(ftk::Format("Media reference keys: {0}").arg(keys.size()));
                if (!keys.empty())
                {
                    player->setMediaReferenceKey(keys.front());
                }
                player->setAudioDevice(AudioDeviceID());
                FTK_CHECK(player->getContext());
                _print(ftk::Format("Objects: {0}").arg(player->getObjectCount()));
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }

        //! A player comparing one timeline with another, which is how the
        //! comparison reaches the player rather than the renderer.
        void PlayerTest::_compare()
        {
            try
            {
                const ftk::Path path(
                    TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                auto player = Player::create(
                    _context,
                    Timeline::create(_context, path));

                std::vector<std::shared_ptr<Timeline> > compare;
                auto compareObserver =
                    ftk::ListObserver<std::shared_ptr<Timeline> >::create(
                        player->observeCompare(),
                        [&compare](const std::vector<std::shared_ptr<Timeline> >& value)
                        {
                            compare = value;
                        });
                player->setCompare({ Timeline::create(_context, path) });
                FTK_CHECK(1 == compare.size());

                CompareTime compareTime = CompareTime::Relative;
                auto compareTimeObserver = ftk::Observer<CompareTime>::create(
                    player->observeCompareTime(),
                    [&compareTime](CompareTime value) { compareTime = value; });
                for (auto i : getCompareTimeEnums())
                {
                    player->setCompareTime(i);
                    FTK_CHECK(i == player->getCompareTime());
                    FTK_CHECK(i == compareTime);
                }

                player->setCompareVideoLayers({ 0 });
                player->forward();
                waitForPlayback();
                player->stop();

                // And with the comparison taken away again.
                player->setCompare({});
                FTK_CHECK(compare.empty());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }
    }
}
