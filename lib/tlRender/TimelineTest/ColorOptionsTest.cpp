// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/ColorOptionsTest.h>

#include <tlRender/Timeline/ColorOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

namespace tl
{
    namespace timeline_tests
    {
        ColorOptionsTest::ColorOptionsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::ColorOptionsTest")
        {}

        std::shared_ptr<ColorOptionsTest> ColorOptionsTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ColorOptionsTest>(new ColorOptionsTest(context));
        }

        void ColorOptionsTest::run()
        {
            {
                FTK_TEST_ENUM(OCIOConfig);
                FTK_TEST_ENUM(LUTOrder);
            }
            {
                _print("LUT formats: " + ftk::join(getLUTFormatNames(), ", "));
                _print("LUT format extensions: " + ftk::join(getLUTFormatExts(), ", "));
            }
            {
                OCIOOptions a;
                OCIOOptions b;
                FTK_CHECK(a == b);
                a.fileName = "fileName";
                FTK_CHECK(a != b);
            }
        }
    }
}
