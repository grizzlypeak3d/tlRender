// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/CompareOptionsTest.h>

#include <tlRender/Timeline/CompareOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

namespace tl
{
    namespace timeline_tests
    {
        CompareOptionsTest::CompareOptionsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::CompareOptionsTest")
        {}

        std::shared_ptr<CompareOptionsTest> CompareOptionsTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<CompareOptionsTest>(new CompareOptionsTest(context));
        }

        void CompareOptionsTest::run()
        {
            {
                FTK_TEST_ENUM(Compare);
                FTK_TEST_ENUM(CompareTime);
            }
            {
                CompareOptions options;
                options.compare = Compare::B;
                FTK_ASSERT(options == options);
                FTK_ASSERT(options != CompareOptions());
            }
            {
                const std::vector<ftk::ImageInfo> infos =
                {
                    ftk::ImageInfo(1920, 1080, ftk::ImageType::RGBA_U8),
                    ftk::ImageInfo(1920 / 2, 1080 / 2, ftk::ImageType::RGBA_U8),
                    ftk::ImageInfo(1920 / 2, 1080 / 2, ftk::ImageType::RGBA_U8),
                    ftk::ImageInfo(1920 / 2, 1080 / 2, ftk::ImageType::RGBA_U8)
                };

                for (auto compare :
                    {
                        Compare::A,
                        Compare::B,
                        Compare::Wipe,
                        Compare::Overlay,
                        Compare::Difference
                    })
                {
                    auto boxes = getBoxes({ compare }, AspectRatioOptions(), infos);
                    FTK_ASSERT(!boxes.empty());
                    FTK_ASSERT(ftk::Box2I(0, 0, 1920, 1080) == boxes[0]);
                    FTK_ASSERT(ftk::Box2I(0, 0, 1920, 1080) == boxes[1]);
                    auto renderSize = getRenderSize({ compare }, AspectRatioOptions(), infos);
                    FTK_ASSERT(ftk::Size2I(1920, 1080) == renderSize);
                }

                auto boxes = getBoxes({ Compare::Horizontal }, AspectRatioOptions(), infos);
                FTK_ASSERT(2 == boxes.size());
                FTK_ASSERT(ftk::Box2I(0, 0, 1920, 1080) == boxes[0]);
                FTK_ASSERT(ftk::Box2I(1920, 0, 1920, 1080) == boxes[1]);
                auto renderSize = getRenderSize({ Compare::Horizontal }, AspectRatioOptions(), infos);
                FTK_ASSERT(ftk::Size2I(1920 * 2, 1080) == renderSize);

                boxes = getBoxes({ Compare::Vertical }, AspectRatioOptions(), infos);
                FTK_ASSERT(2 == boxes.size());
                FTK_ASSERT(ftk::Box2I(0, 0, 1920, 1080) == boxes[0]);
                FTK_ASSERT(ftk::Box2I(0, 1080, 1920, 1080) == boxes[1]);
                renderSize = getRenderSize({ Compare::Vertical }, AspectRatioOptions(), infos);
                FTK_ASSERT(ftk::Size2I(1920, 1080 * 2) == renderSize);

                boxes = getBoxes({ Compare::Tile }, AspectRatioOptions(), infos);
                FTK_ASSERT(4 == boxes.size());
                FTK_ASSERT(ftk::Box2I(0, 0, 1920, 1080) == boxes[0]);
                FTK_ASSERT(ftk::Box2I(1920, 0, 1920, 1080) == boxes[1]);
                FTK_ASSERT(ftk::Box2I(0, 1080, 1920, 1080) == boxes[2]);
                FTK_ASSERT(ftk::Box2I(1920, 1080, 1920, 1080) == boxes[3]);
                renderSize = getRenderSize({ Compare::Tile }, AspectRatioOptions(), infos);
                FTK_ASSERT(ftk::Size2I(1920 * 2, 1080 * 2) == renderSize);
            }
            {
                const auto time = getCompareTime(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, 24.0),
                        OTIO_NS::RationalTime(24.0, 24.0)),
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, 24.0),
                        OTIO_NS::RationalTime(24.0, 24.0)),
                    CompareTime::Absolute);
                FTK_ASSERT(time == OTIO_NS::RationalTime(0.0, 24.0));
            }
            {
                const auto time = getCompareTime(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, 24.0),
                        OTIO_NS::RationalTime(24.0, 24.0)),
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(24.0, 24.0),
                        OTIO_NS::RationalTime(24.0, 24.0)),
                    CompareTime::Relative);
                FTK_ASSERT(time == OTIO_NS::RationalTime(24.0, 24.0));
            }
            {
                CompareTimeOptions options;
                options.alignInPoints = true;
                options.frameOffset = 2;
                FTK_ASSERT(options != CompareTimeOptions());
                const OTIO_NS::TimeRange sourceRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(240.0, 24.0));
                const OTIO_NS::TimeRange compareRange(
                    OTIO_NS::RationalTime(100.0, 24.0),
                    OTIO_NS::RationalTime(240.0, 24.0));
                const auto time = getCompareTime(
                    OTIO_NS::RationalTime(20.0, 24.0),
                    sourceRange,
                    compareRange,
                    CompareTime::Relative,
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(10.0, 24.0),
                        OTIO_NS::RationalTime(100.0, 24.0)),
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(130.0, 24.0),
                        OTIO_NS::RationalTime(100.0, 24.0)),
                    options);
                FTK_ASSERT(time == OTIO_NS::RationalTime(102.0, 24.0));

                const auto absoluteTime = getCompareTime(
                    OTIO_NS::RationalTime(20.0, 24.0),
                    sourceRange,
                    compareRange,
                    CompareTime::Absolute,
                    sourceRange,
                    compareRange,
                    options);
                FTK_ASSERT(absoluteTime == OTIO_NS::RationalTime(20.0, 24.0));
            }
        }
    }
}
