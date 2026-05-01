// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <TimelineTest/TimelineTest.h>

#include <tlRender/Timeline/Timeline.h>
#include <tlRender/Timeline/Util.h>

#include <tlRender/IO/System.h>

#include <ftk/Core/Format.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/timeline.h>

namespace tl
{
    namespace timeline_tests
    {
        TimelineTest::TimelineTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::TimelineTest")
        {}

        std::shared_ptr<TimelineTest> TimelineTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<TimelineTest>(new TimelineTest(context));
        }

        void TimelineTest::run()
        {
            _enums();
            _options();
            _util();
            _transitions();
            _videoData();
            _timeline();
            _separateAudio();
        }

        void TimelineTest::_enums()
        {
            _enum<ImageSeqAudio>("ImageSeqAudio", getImageSeqAudioEnums);
            _enum<Transition>("Transition", getTransitionEnums);
        }

        void TimelineTest::_options()
        {
            Options a;
            a.imageSeqAudio = ImageSeqAudio::FileName;
            FTK_ASSERT(a == a);
            FTK_ASSERT(a != Options());
        }

        void TimelineTest::_util()
        {
        }

        void TimelineTest::_transitions()
        {
            {
                FTK_ASSERT(toTransition(std::string()) == Transition::None);
                FTK_ASSERT(toTransition("SMPTE_Dissolve") == Transition::Dissolve);
            }
        }

        void TimelineTest::_videoData()
        {
            {
                VideoLayer a, b;
                FTK_ASSERT(a == b);
                a.transition = Transition::Dissolve;
                FTK_ASSERT(a != b);
            }
            {
                VideoData a, b;
                FTK_ASSERT(a == b);
                a.time = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_ASSERT(a != b);
            }
        }

        void TimelineTest::_timeline()
        {
            // Test timelines.
            const std::vector<ftk::Path> paths =
            {
                ftk::Path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg"),
#if defined(TLRENDER_FFMPEG) || defined(TLRENDER_FFMPEG_PLUGIN)
                ftk::Path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MovieAndSeq.otio"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "TransitionGap.otio"),
#endif // TLRENDER_FFMPEG or TLRENDER_FFMPEG_PLUGIN
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
                    auto timeline = Timeline::create(_context, path);
                    _timeline(timeline);
                }
                catch (const std::exception& e)
                {
                    _printError(e.what());
                }
            }
            for (const auto& path : paths)
            {
                try
                {
                    _print(ftk::Format("Memory timeline: {0}").arg(path.get()));
                    auto otioTimeline = tl::create(_context, path);
                    toMemRefs(otioTimeline, path.getDir(), ToMemRef::Shared);
                    auto timeline = Timeline::create(_context, otioTimeline);
                    _timeline(timeline);
                }
                catch (const std::exception& e)
                {
                    _printError(e.what());
                }
            }
        }

        void TimelineTest::_timeline(const std::shared_ptr<Timeline>& timeline)
        {
            // Get video from the timeline.
            const OTIO_NS::TimeRange& timeRange = timeline->getTimeRange();
            std::vector<VideoFrame> videoFrame;
            std::vector<VideoRequest> videoRequests;
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0)));
            }
            IOOptions ioOptions;
            ioOptions["Layer"] = "1";
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0), ioOptions));
            }
            while (videoFrame.size() < static_cast<size_t>(timeRange.duration().value()) * 2)
            {
                auto i = videoRequests.begin();
                while (i != videoRequests.end())
                {
                    if (i->future.valid() &&
                        i->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        videoFrame.push_back(i->future.get());
                        i = videoRequests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            FTK_ASSERT(videoRequests.empty());

            // Get audio from the timeline.
            std::vector<AudioFrame> audioFrame;
            std::vector<AudioRequest> audioRequests;
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()); ++i)
            {
                audioRequests.push_back(timeline->getAudio(i));
            }
            while (audioFrame.size() < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()))
            {
                auto i = audioRequests.begin();
                while (i != audioRequests.end())
                {
                    if (i->future.valid() &&
                        i->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        audioFrame.push_back(i->future.get());
                        i = audioRequests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            FTK_ASSERT(audioRequests.empty());

            // Cancel requests.
            videoFrame.clear();
            videoRequests.clear();
            audioFrame.clear();
            audioRequests.clear();
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0)));
            }
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0), ioOptions));
            }
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()); ++i)
            {
                audioRequests.push_back(timeline->getAudio(i));
            }
            std::vector<uint64_t> ids;
            for (const auto& i : videoRequests)
            {
                ids.push_back(i.id);
            }
            for (const auto& i : audioRequests)
            {
                ids.push_back(i.id);
            }
            timeline->cancelRequests(ids);
        }

        void TimelineTest::_separateAudio()
        {
#if defined(TLRENDER_FFMPEG) || defined(TLRENDER_FFMPEG_PLUGIN)
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                const ftk::Path audioPath(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.wav");
                auto timeline = Timeline::create(_context, path.get(), audioPath.get());
            }
            catch (const std::exception& e)
            {
                _printError(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                const ftk::Path audioPath(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.wav");
                auto timeline = Timeline::create(_context, path, audioPath);
            }
            catch (const std::exception& e)
            {
                _printError(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::None;
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_ASSERT(audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _printError(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::Ext;
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_ASSERT(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _printError(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::FileName;
                options.imageSeqAudioFileName = ftk::Path(
                    TLRENDER_SAMPLE_DATA, "AudioToneStereo.wav").get();
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_ASSERT(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _printError(e.what());
            }
#endif // TLRENDER_FFMPEG or TLRENDER_FFMPEG_PLUGIN
        }
    }
}
