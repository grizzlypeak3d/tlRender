// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/DisplayOptionsTest.h>

#include <tlRender/Timeline/DisplayOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

namespace tl
{
    namespace timeline_tests
    {
        DisplayOptionsTest::DisplayOptionsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::DisplayOptionsTest")
        {}

        std::shared_ptr<DisplayOptionsTest> DisplayOptionsTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<DisplayOptionsTest>(new DisplayOptionsTest(context));
        }

        void DisplayOptionsTest::run()
        {
            {
                Color color;
                color.enabled = true;
                FTK_CHECK(color == color);
                FTK_CHECK(color != Color());
            }
            {
                Color color;
                color.brightness = ftk::V3F(2.F, 1.F, 1.F);
                color.contrast = ftk::V3F(2.F, 1.F, 1.F);
                color.saturation = ftk::V3F(2.F, 1.F, 1.F);
                color.hue = .5F;
                const ftk::V3F v(1.F, 1.F, 1.F);
                const auto m = tl::color(color);
                _print(ftk::Format("{0} color: {1}").arg(v).arg(m * v));
            }
            {
                Levels levels;
                levels.enabled = true;
                FTK_CHECK(levels == levels);
                FTK_CHECK(levels != Levels());
            }
            {
                Exposure exposure;
                exposure.enabled = true;
                FTK_CHECK(exposure == exposure);
                FTK_CHECK(exposure != Exposure());
            }
            {
                SoftClip softClip;
                softClip.enabled = true;
                FTK_CHECK(softClip == softClip);
                FTK_CHECK(softClip != SoftClip());
            }
            {
                AspectRatio v;
                FTK_CHECK(parseAspectRatio("16:9", v));
                FTK_CHECK(AspectRatio(16.F, 9.F) == v);
                FTK_CHECK("16:9" == getLabel(v));
                FTK_CHECK(parseAspectRatio("2.39", v));
                FTK_CHECK(AspectRatio(2.39F, 1.F) == v);
                FTK_CHECK("2.39:1" == getLabel(v));
                FTK_CHECK(parseAspectRatio(" 1.85 : 1 ", v));
                FTK_CHECK(AspectRatio(1.85F, 1.F) == v);
                for (const std::string& bad : { "", ":", "16:", ":9", "0", "16:0", "-2", "a", "1:2:3", "2.39x" })
                {
                    FTK_CHECK(!parseAspectRatio(bad, v));
                    FTK_CHECK(AspectRatio(1.85F, 1.F) == v);
                }
            }
            {
                DisplayOptions displayOptions;
                displayOptions.channels = ftk::ChannelDisplay::Red;
                FTK_CHECK(displayOptions == displayOptions);
                FTK_CHECK(displayOptions != DisplayOptions());
            }
        }
    }
}
