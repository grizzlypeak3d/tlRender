// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/UtilTest.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/imageSequenceReference.h>

namespace tl
{
    namespace timeline_tests
    {
        UtilTest::UtilTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::UtilTest")
        {}

        std::shared_ptr<UtilTest> UtilTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<UtilTest>(new UtilTest(context));
        }

        void UtilTest::run()
        {
            _enums();
            _exts();
            _ranges();
            _loop();
            _util();
            _audio();
            _paths();
        }

        void UtilTest::_paths()
        {
            // The base of an image sequence may be written with or without a
            // trailing separator; OTIO accepts both, so both must give the
            // same path.
            const std::string directory = "/tmp";
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::ImageSequenceReference>
                withSeparator(new OTIO_NS::ImageSequenceReference(
                    "seq/", "frame.", ".exr", 1, 1, 24.0, 4));
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::ImageSequenceReference>
                withoutSeparator(new OTIO_NS::ImageSequenceReference(
                    "seq", "frame.", ".exr", 1, 1, 24.0, 4));
            const ftk::Path a = getPath(
                withSeparator.value, directory, ftk::PathOptions());
            const ftk::Path b = getPath(
                withoutSeparator.value, directory, ftk::PathOptions());
            _print(ftk::Format("Sequence path: {0}").arg(a.get()));
            _print(ftk::Format("Sequence path: {0}").arg(b.get()));
            FTK_ASSERT(a.get() == b.get());
        }

        void UtilTest::_enums()
        {
            FTK_TEST_ENUM(CacheDir);
        }

        void UtilTest::_exts()
        {
            for (const auto& i : getExts(
                _context,
                static_cast<int>(FileType::Media) |
                static_cast<int>(FileType::Seq)))
            {
                std::stringstream ss;
                ss << "Timeline extension: " << i;
                _print(ss.str());
            }
            for (const auto& path : getPaths(
                _context,
                ftk::Path(TLRENDER_SAMPLE_DATA),
                ftk::DirListOptions()))
            {
                _print(ftk::Format("Path: {0}").arg(path.get()));
            }
        }

        void UtilTest::_ranges()
        {
            {
                std::vector<OTIO_NS::RationalTime> f;
                auto r = toRanges(f);
                FTK_ASSERT(r.empty());
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(1 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(1, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(1 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24),
                    OTIO_NS::RationalTime(2, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(1 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(3, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(2, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(2 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(1, 24)) == r[0]);
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(2, 24), OTIO_NS::RationalTime(1, 24)) == r[1]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24),
                    OTIO_NS::RationalTime(3, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(2 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(3, 24), OTIO_NS::RationalTime(1, 24)) == r[1]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24),
                    OTIO_NS::RationalTime(3, 24),
                    OTIO_NS::RationalTime(4, 24)
                };
                auto r = toRanges(f);
                FTK_ASSERT(2 == r.size());
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
                FTK_ASSERT(OTIO_NS::TimeRange(OTIO_NS::RationalTime(3, 24), OTIO_NS::RationalTime(2, 24)) == r[1]);
            }
        }

        void UtilTest::_loop()
        {
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                bool looped = false;
                OTIO_NS::RationalTime t = loop(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    range,
                    &looped);
                FTK_ASSERT(OTIO_NS::RationalTime(0.0, 24.0) == t);
                FTK_ASSERT(!looped);
                t = loop(
                    OTIO_NS::RationalTime(24.0, 24.0),
                    range,
                    &looped);
                FTK_ASSERT(OTIO_NS::RationalTime(0.0, 24.0) == t);
                FTK_ASSERT(looped);
                t = loop(
                    OTIO_NS::RationalTime(-1.0, 24.0),
                    range,
                    &looped);
                FTK_ASSERT(OTIO_NS::RationalTime(23.0, 24.0) == t);
                FTK_ASSERT(looped);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_ASSERT(1 == looped.size());
                FTK_ASSERT(range == looped[0]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(-12.0, 24.0),
                    OTIO_NS::RationalTime(48.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_ASSERT(1 == looped.size());
                FTK_ASSERT(bounds == looped[0]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(-12.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_ASSERT(2 == looped.size());
                FTK_ASSERT(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
                FTK_ASSERT(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[1]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_ASSERT(2 == looped.size());
                FTK_ASSERT(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
                FTK_ASSERT(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[1]);
            }
            {
                const ftk::Range<int64_t> range(0, 23);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_ASSERT(1 == looped.size());
                FTK_ASSERT(range == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(-12, 35);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_ASSERT(1 == looped.size());
                FTK_ASSERT(bounds == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(-12, 11);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_ASSERT(2 == looped.size());
                FTK_ASSERT(ftk::Range<int64_t>(12, 23) == looped[0]);
                FTK_ASSERT(ftk::Range<int64_t>(0, 11) == looped[1]);
            }
            {
                const ftk::Range<int64_t> range(12, 35);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_ASSERT(2 == looped.size());
                FTK_ASSERT(ftk::Range<int64_t>(12, 23) == looped[0]);
                FTK_ASSERT(ftk::Range<int64_t>(0, 11) == looped[1]);
            }
        }

        void UtilTest::_util()
        {
            {
                auto otioClip = new OTIO_NS::Clip;
                OTIO_NS::ErrorStatus errorStatus;
                auto otioTrack = new OTIO_NS::Track();
                otioTrack->append_child(otioClip, &errorStatus);
                if (OTIO_NS::is_error(errorStatus))
                {
                    throw std::runtime_error("Cannot append child");
                }
                auto otioStack = new OTIO_NS::Stack;
                otioStack->append_child(otioTrack, &errorStatus);
                if (OTIO_NS::is_error(errorStatus))
                {
                    throw std::runtime_error("Cannot append child");
                }
                OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(new OTIO_NS::Timeline);
                otioTimeline->set_tracks(otioStack);
                FTK_ASSERT(otioStack == getRoot(otioClip));
                FTK_ASSERT(otioStack == getParent<OTIO_NS::Stack>(otioClip));
                FTK_ASSERT(otioTrack == getParent<OTIO_NS::Track>(otioClip));
            }
            {
                VideoFrame a;
                a.time = OTIO_NS::RationalTime(1.0, 24.0);
                VideoFrame b;
                b.time = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_ASSERT(isTimeEqual(a, b));
            }
        }

        void UtilTest::_audio()
        {
            {
                AudioInfo info(2, AudioType::S32, 48000);
                std::vector<AudioFrame> frames;
                auto out = audioCopy(info, frames, Playback::Forward, 0, 2000);
                FTK_ASSERT(out.empty());

                auto audio = Audio::create(info, info.sampleRate);
                int32_t* audioP = reinterpret_cast<int32_t*>(audio->getData());
                for (int i = 0; i < info.sampleRate; ++i, audioP += 2)
                {
                    audioP[0] = i;
                    audioP[1] = i + 1;
                }
                frames.push_back(AudioFrame({ 0.0, { { audio } } }));
                out = audioCopy(info, frames, Playback::Forward, 0, 2000);
                FTK_ASSERT(1 == out.size());
                FTK_ASSERT(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                for (size_t i = 0; i < out[0]->getSampleCount(); ++i, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int32_t>(i) == audioP[0]);
                    FTK_ASSERT(static_cast<int32_t>(i + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Forward, info.sampleRate - 1000, 2000);
                FTK_ASSERT(1 == out.size());
                FTK_ASSERT(1000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                for (size_t i = 0, j = info.sampleRate - 1000; i < out[0]->getSampleCount(); ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }

                frames.push_back(AudioFrame({ 1.0, { { audio } } }));
                out = audioCopy(info, frames, Playback::Forward, info.sampleRate - 1000, 2000);
                FTK_ASSERT(1 == out.size());
                FTK_ASSERT(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                size_t i = 0;
                size_t j = info.sampleRate - 1000;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }
                i = 0;
                j = 0;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Reverse, info.sampleRate, 2000);
                FTK_ASSERT(1 == out.size());
                FTK_ASSERT(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                i = 0;
                j = info.sampleRate - 2000;
                for (; i < 2000; ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Reverse, info.sampleRate + 1000, 2000);
                FTK_ASSERT(1 == out.size());
                FTK_ASSERT(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                i = 0;
                j = info.sampleRate - 1000;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }
                i = 0;
                j = 0;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_ASSERT(static_cast<int>(j) == audioP[0]);
                    FTK_ASSERT(static_cast<int>(j + 1) == audioP[1]);
                }
            }
        }
    }
}
