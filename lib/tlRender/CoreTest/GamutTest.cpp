// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/GamutTest.h>

#include <tlRender/Core/Gamut.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <cstdlib>

namespace tl
{
    namespace core_tests
    {
        GamutTest::GamutTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::GamutTest")
        {}

        std::shared_ptr<GamutTest> GamutTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<GamutTest>(new GamutTest(context));
        }

        void GamutTest::run()
        {
            FTK_CHECK(isDisplayP3({
                { "Color Primaries", "smpte432" },
                { "Color Transfer", "iec61966-2-1" } }));
            FTK_CHECK(!isDisplayP3({ { "Color Primaries", "smpte432" } }));
            FTK_CHECK(!isDisplayP3({
                { "Color Primaries", "bt709" },
                { "Color Transfer", "iec61966-2-1" } }));
            FTK_CHECK(!isDisplayP3({}));

            // P3 pixels, and what sRGB makes of them: its red past sRGB's,
            // clipped to sRGB's; a duller P3 red brighter in sRGB's terms;
            // sRGB's red, which in P3 is orange-ish; white and grey as they
            // are, and the alpha untouched.
            const uint8_t in[][4] =
            {
                { 255, 0, 0, 255 },
                { 200, 0, 0, 255 },
                { 234, 51, 35, 128 },
                { 255, 255, 255, 255 },
                { 128, 128, 128, 255 }
            };
            const uint8_t expected[][4] =
            {
                { 255, 0, 0, 255 },
                { 219, 0, 0, 255 },
                { 255, 0, 0, 128 },
                { 255, 255, 255, 255 },
                { 128, 128, 128, 255 }
            };
            const size_t count = sizeof(in) / sizeof(in[0]);
            auto image = ftk::Image::create(static_cast<int>(count), 1, ftk::ImageType::RGBA_U8);
            for (size_t i = 0; i < count; ++i)
            {
                for (int c = 0; c < 4; ++c)
                {
                    image->getData()[i * 4 + c] = in[i][c];
                }
            }
            displayP3ToSRGB(image);
            for (size_t i = 0; i < count; ++i)
            {
                const uint8_t* p = image->getData() + i * 4;
                _print(ftk::Format("{0} {1} {2} -> {3} {4} {5}").
                    arg(in[i][0]).arg(in[i][1]).arg(in[i][2]).
                    arg(p[0]).arg(p[1]).arg(p[2]));
                for (int c = 0; c < 4; ++c)
                {
                    FTK_CHECK(std::abs(p[c] - expected[i][c]) <= 1);
                }
            }

            // Another type is left as it is.
            auto other = ftk::Image::create(1, 1, ftk::ImageType::RGB_U8);
            other->getData()[0] = 200;
            displayP3ToSRGB(other);
            FTK_CHECK(200 == other->getData()[0]);
            displayP3ToSRGB(nullptr);
        }
    }
}
