// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/CPUColorTest.h>

#include <tlRender/Timeline/CPUColor.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <algorithm>
#include <cmath>
#include <vector>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#endif // TLRENDER_OCIO

namespace tl
{
    namespace timeline_tests
    {
        CPUColorTest::CPUColorTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::CPUColorTest")
        {}

        std::shared_ptr<CPUColorTest> CPUColorTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<CPUColorTest>(new CPUColorTest(context));
        }

        void CPUColorTest::run()
        {
            _table();
        }

        void CPUColorTest::_table()
        {
#if defined(TLRENDER_OCIO)
            // The table against the color it was sampled from, on bytes
            // across the whole cube, through the default configuration's
            // display and view: an ACES 2.0 output, the curve a table has
            // the most trouble following. Nearly every pixel is within a
            // step or two of it. The few that are not are where the color
            // itself all but jumps: a bright saturated color going to white
            // once a channel clips -- in ACEScct a yellow gains a fifth of
            // its blue between 180 and 182 -- and, with a linear input, the
            // shadows, where one byte is already a large step.
            namespace OCIO = OCIO_NAMESPACE;
            auto config = OCIO::Config::CreateFromFile("ocio://default");
            for (const std::string& input : { "ACEScct", "ACEScg" })
            {
                OCIOOptions ocio;
                ocio.enabled = true;
                ocio.input = input;
                ocio.display = config->getDefaultDisplay();
                ocio.view = config->getDefaultView(ocio.display.c_str());
                const CPUColor color(ocio, LUTOptions(), DisplayOptions(), true);
                const CPUColorTable table(color);

                std::vector<uint8_t> bytes;
                for (int b = 0; b < 256; b += 5)
                {
                    for (int g = 0; g < 256; g += 5)
                    {
                        for (int r = 0; r < 256; r += 5)
                        {
                            bytes.push_back(r);
                            bytes.push_back(g);
                            bytes.push_back(b);
                            bytes.push_back((r + g + b) % 256);
                        }
                    }
                }
                const size_t count = bytes.size() / 4;
                std::vector<float> expected(bytes.size());
                for (size_t i = 0; i < bytes.size(); ++i)
                {
                    expected[i] = bytes[i] / 255.F;
                }
                color.apply(expected.data(), count);
                std::vector<uint8_t> result = bytes;
                table.apply(result.data(), count);

                int maxDiff = 0;
                double sumDiff = 0.0;
                size_t close = 0;
                bool alpha = true;
                for (size_t i = 0; i < count; ++i)
                {
                    for (size_t j = 0; j < 3; ++j)
                    {
                        const int e = static_cast<int>(
                            std::clamp(expected[i * 4 + j], 0.F, 1.F) * 255.F + .5F);
                        const int d = std::abs(e - result[i * 4 + j]);
                        maxDiff = std::max(maxDiff, d);
                        sumDiff += d;
                        close += d <= 2;
                    }
                    // Alpha is passed through.
                    alpha &= result[i * 4 + 3] == bytes[i * 4 + 3];
                }
                const double meanDiff = sumDiff / (count * 3);
                const double closeShare = close / static_cast<double>(count * 3);
                _print(ftk::Format("{0}: {1} pixels, mean difference {2}/255, "
                    "{3}% within 2/255, largest {4}/255").
                    arg(input).
                    arg(count).
                    arg(meanDiff, 3).
                    arg(closeShare * 100.0, 2).
                    arg(maxDiff));
                FTK_CHECK(meanDiff < .5);
                FTK_CHECK(closeShare > .97);
                FTK_CHECK(alpha);
            }
#endif // TLRENDER_OCIO
        }
    }
}
