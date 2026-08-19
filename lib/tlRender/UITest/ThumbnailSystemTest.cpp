// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UITest/ThumbnailSystemTest.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/IO/System.h>

#include <tlRender/Timeline/Timeline.h>
#include <tlRender/Timeline/Util.h>

#include <opentimelineio/clip.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

#include <ftk/Core/Image.h>
#include <ftk/Core/Path.h>
#include <ftk/Core/String.h>

namespace tl
{
    namespace ui_tests
    {
        ThumbnailSystemTest::ThumbnailSystemTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "ui_tests::ThumbnailSystemTest")
        {}

        std::shared_ptr<ThumbnailSystemTest> ThumbnailSystemTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ThumbnailSystemTest>(new ThumbnailSystemTest(context));
        }

        void ThumbnailSystemTest::run()
        {
            _gapSeq();
            _seqFrame();
            auto thumbnailSystem = _context->getSystem<ui::ThumbnailSystem>();
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
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "StreamedSeq.otioz"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MixedSeq.otioz")
            };
            for (const auto& path : paths)
            {
                std::vector<ui::InfoRequest> infoRequests;
                std::vector<ui::ThumbnailRequest> thumbnailRequests;
                std::vector<ui::WaveformRequest> waveformRequests;
                bool mediaReadable = false;
                try
                {
                    auto timeline = Timeline::create(_context, path);
                    for (const auto& clip : timeline->getTimeline()->find_clips())
                    {
                        const auto mediaPath = getPath(
                            clip->media_reference(),
                            timeline->getPath().getDir(),
                            ftk::PathOptions());
                        // Named by the timeline it lives in, so that media
                        // held as byte ranges in a bundle is reachable.
                        const auto& timelinePath = timeline->getPath();
                        // A bundle opens whatever its media is, so the
                        // requests can be made either way; only a build with
                        // the media's format can answer them.
                        mediaReadable |= _context->getSystem<ReadSystem>()->
                            getPlugin(mediaPath) != nullptr;
                        infoRequests.push_back(thumbnailSystem->getInfo(
                            timelinePath,
                            mediaPath));
                        thumbnailRequests.push_back(thumbnailSystem->getThumbnail(
                            timelinePath,
                            mediaPath,
                            100));
                        waveformRequests.push_back(thumbnailSystem->getWaveform(
                            timelinePath,
                            mediaPath,
                            ftk::Size2I(200, 100)));
                    }
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                }
                // Draining the futures is not enough: media named inside a
                // bundle has to actually come back, since reaching it is the
                // whole point of naming the timeline as well.
                size_t infoCount = 0;
                for (auto& request : infoRequests)
                {
                    const auto info = request.future.get();
                    if (!info.video.empty() || info.audio.channelCount > 0)
                    {
                        ++infoCount;
                    }
                }
                FTK_CHECK(!mediaReadable || infoCount > 0);
                size_t thumbnailCount = 0;
                for (auto& request : thumbnailRequests)
                {
                    const auto thumbnail = request.future.get();
                    if (thumbnail)
                    {
                        ++thumbnailCount;
                    }
                }
                FTK_CHECK(!mediaReadable || thumbnailCount > 0);
                for (auto& request : waveformRequests)
                {
                    const auto waveform = request.future.get();
                }
            }
        }

        namespace
        {
            ftk::Path mediaPathFor(const std::shared_ptr<Timeline>& timeline)
            {
                return getPath(
                    timeline->getTimeline()->find_clips()[0]->media_reference(),
                    timeline->getPath().getDir(),
                    ftk::PathOptions());
            }
        }

        void ThumbnailSystemTest::_seqFrame()
        {
            // A thumbnail is of the file it names. One frame of a sequence,
            // asked for on its own, must not be expanded into the sequence it
            // belongs to: every frame in a directory would come back as the
            // first one, which is what a directory listed without sequences
            // asks for.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const auto framePath = [this](int64_t frame)
                {
                    return ftk::Path((_getTempDir() /
                        ftk::Format("ThumbFrame.000{0}.png").arg(frame).str()).u8string());
                };
            auto writePlugin = writeSystem->getPlugin(framePath(1));
            if (!writePlugin || !readSystem->getPlugin(framePath(1)))
            {
                return;
            }
            IOInfo writeInfo;
            writeInfo.video.push_back(writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(16, 16), ftk::ImageType::RGB_U8)));
            {
                auto write = writeSystem->write(framePath(1), writeInfo);
                for (int64_t frame = 1; frame <= 4; ++frame)
                {
                    auto image = ftk::Image::create(writeInfo.video[0]);
                    // Every frame a different value, so that a thumbnail of
                    // the wrong frame cannot pass for the right one.
                    std::memset(
                        image->getData(),
                        static_cast<int>(frame * 60),
                        image->getByteCount());
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                        image);
                }
            }

            auto thumbnailSystem = _context->getSystem<ui::ThumbnailSystem>();
            std::vector<int> values;
            for (int64_t frame : { 1, 3 })
            {
                const ftk::Path path = framePath(frame);
                // The frame is parsed out of the name, but no range is
                // stated: this is the path a directory listing hands over
                // when it is not collapsing sequences.
                FTK_CHECK(path.hasNum());
                FTK_CHECK(!path.isSeq());
                auto request = thumbnailSystem->getThumbnail(path, 16);
                auto image = request.future.get();
                FTK_CHECK(image);
                values.push_back(image->getData()[0]);
                _print(ftk::Format("Frame {0} thumbnail: {1}").
                    arg(frame).arg(values.back()));
            }
            FTK_CHECK(values[0] != values[1]);
        }

        void ThumbnailSystemTest::_gapSeq()
        {
            // A sequence with a gap, opened over a range wider than the
            // frames on disk. Which times give a thumbnail has to match which
            // frames are actually there.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Path path(
                (_getTempDir() / "ThumbGap.0001.png").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            if (!writePlugin || !readSystem->getPlugin(path))
            {
                return;
            }
            IOInfo writeInfo;
            writeInfo.video.push_back(writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(16, 16), ftk::ImageType::RGB_U8)));
            const std::vector<int64_t> onDisk = { 1, 2, 3, 5 };
            {
                auto write = writeSystem->write(path, writeInfo);
                for (int64_t frame : onDisk)
                {
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                        ftk::Image::create(writeInfo.video[0]));
                }
            }

            ftk::Path wide(path);
            wide.setFrames(ftk::RangeI64(1, 25));
            Options options;
            auto timeline = Timeline::create(_context, wide, options);
            FTK_CHECK(25 == timeline->getTimeRange().duration().value());

            auto thumbnailSystem = _context->getSystem<ui::ThumbnailSystem>();
            const auto mediaPath = mediaPathFor(timeline);

            // Which frames readMedia gives an image for, which is the call
            // the thumbnail system makes. Splits a fault in the read from one
            // in the thumbnail cache above it.
            {
                std::vector<std::string> direct;
                for (int64_t frame = 1; frame <= 25; ++frame)
                {
                    auto r = timeline->readMedia(
                        mediaPath,
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0));
                    bool image = false;
                    try
                    {
                        image = r.valid() && r.get().image != nullptr;
                    }
                    catch (const std::exception&)
                    {}
                    if (image)
                    {
                        direct.push_back(ftk::Format("{0}").arg(frame).str());
                    }
                }
                _print("readMedia images: " + ftk::join(direct, ','));
            }

            std::vector<ui::ThumbnailRequest> requests;
            for (int64_t frame = 1; frame <= 25; ++frame)
            {
                requests.push_back(thumbnailSystem->getThumbnail(
                    timeline->getPath(),
                    mediaPath,
                    16,
                    OTIO_NS::RationalTime(static_cast<double>(frame), 24.0)));
            }
            std::vector<int64_t> withImage;
            for (size_t i = 0; i < requests.size(); ++i)
            {
                if (requests[i].future.get())
                {
                    withImage.push_back(static_cast<int64_t>(i) + 1);
                }
            }
            _print("Thumbnails for frames: " + ftk::join(
                [&withImage]
                {
                    std::vector<std::string> out;
                    for (int64_t i : withImage)
                    {
                        out.push_back(ftk::Format("{0}").arg(i).str());
                    }
                    return out;
                }(), ','));

            // Frame 24 is the one to watch: at 24fps it is one second, which
            // is what the old unset-time marker of -1/-1 was worth. Comparing
            // times rescales them, so a request for it once read as "no time
            // given" and came back with the first frame.
            FTK_CHECK(onDisk == withImage);
        }
    }
}