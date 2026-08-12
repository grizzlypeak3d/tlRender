// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/BackgroundOptionsTest.h>

#include <tlRender/Timeline/BackgroundOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <nlohmann/json.hpp>

namespace tl
{
    namespace timeline_tests
    {
        BackgroundOptionsTest::BackgroundOptionsTest(
            const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::BackgroundOptionsTest")
        {}

        std::shared_ptr<BackgroundOptionsTest> BackgroundOptionsTest::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<BackgroundOptionsTest>(
                new BackgroundOptionsTest(context));
        }

        void BackgroundOptionsTest::run()
        {
            {
                FTK_TEST_ENUM(Background);
            }
            {
                // A field at a time, which is what notices one left out of
                // the comparison.
                Outline v;
                FTK_CHECK(v == Outline());
                v = Outline();
                v.enabled = true;
                FTK_CHECK(v != Outline());
                v = Outline();
                v.width = 100;
                FTK_CHECK(v != Outline());
                v = Outline();
                v.color = ftk::Color4F(0.F, 1.F, 0.F);
                FTK_CHECK(v != Outline());
            }
            {
                BackgroundOptions v;
                FTK_CHECK(v == BackgroundOptions());
                v = BackgroundOptions();
                v.type = Background::Checkers;
                FTK_CHECK(v != BackgroundOptions());
                v = BackgroundOptions();
                v.solidColor = ftk::Color4F(1.F, 0.F, 0.F);
                FTK_CHECK(v != BackgroundOptions());
                v = BackgroundOptions();
                v.checkersColor.first = ftk::Color4F(1.F, 0.F, 0.F);
                FTK_CHECK(v != BackgroundOptions());
                v = BackgroundOptions();
                v.checkersSize = ftk::Size2I(10, 10);
                FTK_CHECK(v != BackgroundOptions());
                v = BackgroundOptions();
                v.gradientColor.second = ftk::Color4F(1.F, 0.F, 0.F);
                FTK_CHECK(v != BackgroundOptions());
                v = BackgroundOptions();
                v.outline.enabled = true;
                FTK_CHECK(v != BackgroundOptions());
            }
            {
                BackgroundOptions v;
                v.type = Background::Gradient;
                v.solidColor = ftk::Color4F(.1F, .2F, .3F);
                v.checkersColor = { ftk::Color4F(.4F, .5F, .6F), ftk::Color4F(.7F, .8F, .9F) };
                v.checkersSize = ftk::Size2I(20, 30);
                v.gradientColor = { ftk::Color4F(1.F, 0.F, 0.F), ftk::Color4F(0.F, 0.F, 1.F) };
                v.outline.enabled = true;
                v.outline.width = 5;
                nlohmann::json json;
                to_json(json, v);
                BackgroundOptions v2;
                from_json(json, v2);
                FTK_CHECK(v == v2);
            }
        }
    }
}
