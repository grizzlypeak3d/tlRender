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
                DisplayOptions displayOptions;
                displayOptions.channels = ftk::ChannelDisplay::Red;
                FTK_CHECK(displayOptions == displayOptions);
                FTK_CHECK(displayOptions != DisplayOptions());
            }
        }
    }
}
