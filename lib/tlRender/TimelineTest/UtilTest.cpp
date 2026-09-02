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
            FTK_CHECK(a.get() == b.get());

            // Gathering the frames of a sequence: naming one frame names the
            // sequence, the same way listing a directory gathers its frames
            // into one entry, and the one flag says both.
            // The extension is named rather than asked of the plugins.
            // Gathering reads the directory and does not open anything, so
            // which formats were compiled in is not part of what this checks
            // -- and the minimal configuration compiles no image plugin at
            // all, so asking would gather nothing.
            const ftk::Path frame(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.png");
            ftk::DirListOptions dirListOptions;
            dirListOptions.seqExts = { ".png" };
            {
                const auto paths = getPaths(_context, frame, dirListOptions);
                FTK_CHECK(1 == paths.size());
                _print(ftk::Format("Gathered: {0}").arg(paths[0].getFrameRange()));
                FTK_CHECK(paths[0].isSeq());
            }
            {
                // Turned off, the frame is the file it names.
                dirListOptions.seq = false;
                const auto paths = getPaths(_context, frame, dirListOptions);
                FTK_CHECK(1 == paths.size());
                FTK_CHECK(!paths[0].isSeq());
            }
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
                FTK_CHECK(r.empty());
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24)
                };
                auto r = toRanges(f);
                FTK_CHECK(1 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(1, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24)
                };
                auto r = toRanges(f);
                FTK_CHECK(1 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24),
                    OTIO_NS::RationalTime(2, 24)
                };
                auto r = toRanges(f);
                FTK_CHECK(1 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(3, 24)) == r[0]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(2, 24)
                };
                auto r = toRanges(f);
                FTK_CHECK(2 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(1, 24)) == r[0]);
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(2, 24), OTIO_NS::RationalTime(1, 24)) == r[1]);
            }
            {
                std::vector<OTIO_NS::RationalTime> f =
                {
                    OTIO_NS::RationalTime(0, 24),
                    OTIO_NS::RationalTime(1, 24),
                    OTIO_NS::RationalTime(3, 24)
                };
                auto r = toRanges(f);
                FTK_CHECK(2 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(3, 24), OTIO_NS::RationalTime(1, 24)) == r[1]);
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
                FTK_CHECK(2 == r.size());
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, 24), OTIO_NS::RationalTime(2, 24)) == r[0]);
                FTK_CHECK(OTIO_NS::TimeRange(OTIO_NS::RationalTime(3, 24), OTIO_NS::RationalTime(2, 24)) == r[1]);
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
                FTK_CHECK(OTIO_NS::RationalTime(0.0, 24.0) == t);
                FTK_CHECK(!looped);
                t = loop(
                    OTIO_NS::RationalTime(24.0, 24.0),
                    range,
                    &looped);
                FTK_CHECK(OTIO_NS::RationalTime(0.0, 24.0) == t);
                FTK_CHECK(looped);
                t = loop(
                    OTIO_NS::RationalTime(-1.0, 24.0),
                    range,
                    &looped);
                FTK_CHECK(OTIO_NS::RationalTime(23.0, 24.0) == t);
                FTK_CHECK(looped);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(range == looped[0]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(-12.0, 24.0),
                    OTIO_NS::RationalTime(48.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(bounds == looped[0]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(-12.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
                FTK_CHECK(OTIO_NS::TimeRange(
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
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[1]);
            }
            {
                const ftk::Range<int64_t> range(0, 23);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(range == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(-12, 35);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(bounds == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(-12, 11);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(ftk::Range<int64_t>(12, 23) == looped[0]);
                FTK_CHECK(ftk::Range<int64_t>(0, 11) == looped[1]);
            }
            {
                const ftk::Range<int64_t> range(12, 35);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(ftk::Range<int64_t>(12, 23) == looped[0]);
                FTK_CHECK(ftk::Range<int64_t>(0, 11) == looped[1]);
            }
            // A range entirely outside the bounds loops into them rather
            // than answering nothing: the player's cache eviction keeps
            // what these ranges contain, and an empty answer erased every
            // frame the fill had just requested.
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(36.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(12.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
            }
            {
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(-24.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0)) == looped[0]);
            }
            {
                // Outside, and straddling the end once looped in.
                const OTIO_NS::TimeRange range(
                    OTIO_NS::RationalTime(42.0, 24.0),
                    OTIO_NS::RationalTime(12.0, 24.0));
                const OTIO_NS::TimeRange bounds(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                const auto looped = loop(range, bounds);
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(18.0, 24.0),
                    OTIO_NS::RationalTime(6.0, 24.0)) == looped[0]);
                FTK_CHECK(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(6.0, 24.0)) == looped[1]);
            }
            {
                const ftk::Range<int64_t> range(36, 47);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(ftk::Range<int64_t>(12, 23) == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(-24, -13);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(1 == looped.size());
                FTK_CHECK(ftk::Range<int64_t>(0, 11) == looped[0]);
            }
            {
                const ftk::Range<int64_t> range(42, 53);
                const ftk::Range<int64_t> bounds(0, 23);
                const auto looped = loop(range, bounds);
                FTK_CHECK(2 == looped.size());
                FTK_CHECK(ftk::Range<int64_t>(18, 23) == looped[0]);
                FTK_CHECK(ftk::Range<int64_t>(0, 5) == looped[1]);
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
                FTK_CHECK(otioStack == getRoot(otioClip));
                FTK_CHECK(otioStack == getParent<OTIO_NS::Stack>(otioClip));
                FTK_CHECK(otioTrack == getParent<OTIO_NS::Track>(otioClip));
            }
            {
                VideoFrame a;
                a.time = OTIO_NS::RationalTime(1.0, 24.0);
                VideoFrame b;
                b.time = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_CHECK(isTimeEqual(a, b));
            }
        }

        void UtilTest::_audio()
        {
            {
                AudioInfo info(2, AudioType::S32, 48000);
                std::vector<AudioFrame> frames;
                auto out = audioCopy(info, frames, Playback::Forward, 0, 2000);
                FTK_CHECK(out.empty());

                auto audio = Audio::create(info, info.sampleRate);
                int32_t* audioP = reinterpret_cast<int32_t*>(audio->getData());
                for (int i = 0; i < info.sampleRate; ++i, audioP += 2)
                {
                    audioP[0] = i;
                    audioP[1] = i + 1;
                }
                frames.push_back(AudioFrame({ 0.0, { { audio } } }));
                out = audioCopy(info, frames, Playback::Forward, 0, 2000);
                FTK_CHECK(1 == out.size());
                FTK_CHECK(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                for (size_t i = 0; i < out[0]->getSampleCount(); ++i, audioP += 2)
                {
                    FTK_CHECK(static_cast<int32_t>(i) == audioP[0]);
                    FTK_CHECK(static_cast<int32_t>(i + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Forward, info.sampleRate - 1000, 2000);
                FTK_CHECK(1 == out.size());
                FTK_CHECK(1000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                for (size_t i = 0, j = info.sampleRate - 1000; i < out[0]->getSampleCount(); ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }

                frames.push_back(AudioFrame({ 1.0, { { audio } } }));
                out = audioCopy(info, frames, Playback::Forward, info.sampleRate - 1000, 2000);
                FTK_CHECK(1 == out.size());
                FTK_CHECK(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                size_t i = 0;
                size_t j = info.sampleRate - 1000;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }
                i = 0;
                j = 0;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Reverse, info.sampleRate, 2000);
                FTK_CHECK(1 == out.size());
                FTK_CHECK(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                i = 0;
                j = info.sampleRate - 2000;
                for (; i < 2000; ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }

                out = audioCopy(info, frames, Playback::Reverse, info.sampleRate + 1000, 2000);
                FTK_CHECK(1 == out.size());
                FTK_CHECK(2000 == out[0]->getSampleCount());
                audioP = reinterpret_cast<int32_t*>(out[0]->getData());
                i = 0;
                j = info.sampleRate - 1000;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }
                i = 0;
                j = 0;
                for (; i < 1000; ++i, ++j, audioP += 2)
                {
                    FTK_CHECK(static_cast<int>(j) == audioP[0]);
                    FTK_CHECK(static_cast<int>(j + 1) == audioP[1]);
                }
            }
        }
    }
}
