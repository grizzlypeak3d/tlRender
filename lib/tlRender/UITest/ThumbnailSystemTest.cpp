// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UITest/ThumbnailSystemTest.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/Timeline/Timeline.h>
#include <tlRender/Timeline/Util.h>

#include <opentimelineio/clip.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

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
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz")
            };
            for (const auto& path : paths)
            {
                std::vector<ui::InfoRequest> infoRequests;
                std::vector<ui::ThumbnailRequest> thumbnailRequests;
                std::vector<ui::WaveformRequest> waveformRequests;
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
                FTK_ASSERT(infoRequests.empty() || infoCount > 0);
                size_t thumbnailCount = 0;
                for (auto& request : thumbnailRequests)
                {
                    const auto thumbnail = request.future.get();
                    if (thumbnail)
                    {
                        ++thumbnailCount;
                    }
                }
                FTK_ASSERT(thumbnailRequests.empty() || thumbnailCount > 0);
                for (auto& request : waveformRequests)
                {
                    const auto waveform = request.future.get();
                }
            }
        }
    }
}
