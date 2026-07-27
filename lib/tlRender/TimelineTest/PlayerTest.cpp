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
#if defined(TLRENDER_FFMPEG_PLUGIN) || defined(TLRENDER_FFMPEG_CMD)
                ftk::Path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MovieAndSeq.otio"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "TransitionGap.otio"),
#endif // TLRENDER_FFMPEG_PLUGIN or TLRENDER_FFMPEG_CMD
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
                    auto timeline = Timeline::create(_context, path.get());
                    auto player = Player::create(_context, timeline);
                    FTK_ASSERT(player->getTimeline());
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
            FTK_ASSERT(doubleSpeed == speed);
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
            FTK_ASSERT(Playback::Forward == player->getPlayback());
            FTK_ASSERT(Playback::Forward == playback);

            // Test the playback loop mode.
            Loop loop = Loop::Loop;
            auto loopObserver = ftk::Observer<Loop>::create(
                player->observeLoop(),
                [&loop](Loop value)
                {
                    loop = value;
                });
            player->setLoop(Loop::Once);
            FTK_ASSERT(Loop::Once == player->getLoop());
            FTK_ASSERT(Loop::Once == loop);

            // Test the current time.
            player->setPlayback(Playback::Stop);
            OTIO_NS::RationalTime currentTime = invalidTime;
            auto currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                player->observeCurrentTime(),
                [&currentTime](const OTIO_NS::RationalTime& value)
                {
                    currentTime = value;
                });
            player->seek(timeRange.start_time());
            FTK_ASSERT(timeRange.start_time() == player->getCurrentTime());
            FTK_ASSERT(timeRange.start_time() == currentTime);
            const double rate = timeRange.duration().rate();
            player->seek(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate));
            FTK_ASSERT(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate) ==
                currentTime);
            player->gotoEnd();
            FTK_ASSERT(timeRange.end_time_inclusive() == currentTime);
            player->gotoStart();
            FTK_ASSERT(timeRange.start_time() == currentTime);
            player->frameNext();
            FTK_ASSERT(
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
            OTIO_NS::TimeRange inOutRange = invalidTimeRange;
            auto inOutRangeObserver = ftk::Observer<OTIO_NS::TimeRange>::create(
                player->observeInOutRange(),
                [&inOutRange](const OTIO_NS::TimeRange& value)
                {
                    inOutRange = value;
                });
            player->setInOutRange(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)));
            FTK_ASSERT(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)) == player->getInOutRange());
            FTK_ASSERT(OTIO_NS::TimeRange(
                timeRange.start_time(),
                OTIO_NS::RationalTime(10.0, rate)) == inOutRange);
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate));
            player->setInPoint();
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(10.0, rate));
            player->setOutPoint();
            FTK_ASSERT(OTIO_NS::TimeRange(
                timeRange.start_time() + OTIO_NS::RationalTime(1.0, rate),
                OTIO_NS::RationalTime(10.0, rate)) == inOutRange);
            player->resetInPoint();
            player->resetOutPoint();
            FTK_ASSERT(OTIO_NS::TimeRange(timeRange.start_time(), timeRange.duration()) == inOutRange);

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
            FTK_ASSERT(ioOptions2 == player->getIOOptions());
            FTK_ASSERT(ioOptions2 == ioOptions);
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
            FTK_ASSERT(videoLayer2 == player->getVideoLayer());
            FTK_ASSERT(videoLayer2 == videoLayer);
            std::vector<int> compareVideoLayers2 = { 2, 3 };
            player->setCompareVideoLayers(compareVideoLayers2);
            FTK_ASSERT(compareVideoLayers2 == player->getCompareVideoLayers());
            FTK_ASSERT(compareVideoLayers2 == compareVideoLayers);
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
            FTK_ASSERT(.5F == player->getVolume());
            FTK_ASSERT(.5F == volume);
            player->setVolume(1.F);

            bool mute = false;
            auto muteObserver = ftk::Observer<bool>::create(
                player->observeMute(),
                [&mute](bool value)
                {
                    mute = value;
                });
            player->setMute(true);
            FTK_ASSERT(player->isMuted());
            FTK_ASSERT(mute);
            player->setMute(false);

            std::vector<bool> channelMute = { false, false };
            auto channelMuteObserver = ftk::ListObserver<bool>::create(
                player->observeChannelMute(),
                [&channelMute](const std::vector<bool>& value)
                {
                    channelMute = value;
                });
            player->setChannelMute({ true, true });
            FTK_ASSERT(player->getChannelMute() == std::vector<bool>({ true, true }));
            FTK_ASSERT(channelMute[0]);
            FTK_ASSERT(channelMute[1]);
            player->setChannelMute({ false, false });

            double audioOffset = 0.0;
            auto audioOffsetObserver = ftk::Observer<double>::create(
                player->observeAudioOffset(),
                [&audioOffset](double value)
                {
                    audioOffset = value;
                });
            player->setAudioOffset(0.5);
            FTK_ASSERT(0.5 == player->getAudioOffset());
            FTK_ASSERT(0.5 == audioOffset);
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
                FTK_ASSERT(cacheOptions == player->getCacheOptions());

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
    }
}
