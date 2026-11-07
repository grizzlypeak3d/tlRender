// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/ICCTest.h>

#include <tlRender/Core/ICC.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <array>
#include <cmath>
#include <string>

namespace tl
{
    namespace core_tests
    {
        namespace
        {
            uint32_t u32(const std::vector<uint8_t>& d, size_t at)
            {
                return
                    static_cast<uint32_t>(d[at]) << 24 |
                    static_cast<uint32_t>(d[at + 1]) << 16 |
                    static_cast<uint32_t>(d[at + 2]) << 8 |
                    static_cast<uint32_t>(d[at + 3]);
            }

            std::string sig(const std::vector<uint8_t>& d, size_t at)
            {
                return std::string(reinterpret_cast<const char*>(d.data() + at), 4);
            }

            //! Where a tag's data is, from the tag table.
            size_t find(const std::vector<uint8_t>& d, const std::string& tag)
            {
                const uint32_t count = u32(d, 128);
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (sig(d, 132 + i * 12) == tag)
                    {
                        return u32(d, 132 + i * 12 + 4);
                    }
                }
                return 0;
            }

            //! An XYZ tag's three numbers.
            std::array<double, 3> xyz(const std::vector<uint8_t>& d, const std::string& tag)
            {
                const size_t at = find(d, tag);
                std::array<double, 3> out{};
                if (at && "XYZ " == sig(d, at))
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        out[i] = static_cast<int32_t>(u32(d, at + 8 + i * 4)) / 65536.0;
                    }
                }
                return out;
            }

            bool near(const std::array<double, 3>& a, const std::array<double, 3>& b)
            {
                for (int i = 0; i < 3; ++i)
                {
                    if (std::abs(a[i] - b[i]) > .0015)
                        return false;
                }
                return true;
            }
        }

        ICCTest::ICCTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::ICCTest")
        {}

        std::shared_ptr<ICCTest> ICCTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ICCTest>(new ICCTest(context));
        }

        void ICCTest::run()
        {
            // Nothing a profile can hold: no colors, an HDR curve, no curve.
            FTK_CHECK(getICCProfile({}).empty());
            FTK_CHECK(getICCProfile({
                { "Color Primaries", "bt2020" },
                { "Color Transfer", "smpte2084" } }).empty());
            FTK_CHECK(getICCProfile({ { "Color Primaries", "bt709" } }).empty());
            FTK_CHECK(getICCProfile({
                { "Chromaticities", "not numbers" },
                { "Color Transfer", "iec61966-2-1" } }).empty());

            // Display P3, as the colors a display transform names; the
            // primaries as Apple's own Display P3 profile has them.
            const auto p3 = getICCProfile({
                { "Color Primaries", "smpte432" },
                { "Color Transfer", "iec61966-2-1" } });
            FTK_CHECK(!p3.empty());
            FTK_CHECK(u32(p3, 0) == p3.size());
            FTK_CHECK(0 == p3.size() % 4);
            FTK_CHECK("mntr" == sig(p3, 12));
            FTK_CHECK("RGB " == sig(p3, 16));
            FTK_CHECK("XYZ " == sig(p3, 20));
            FTK_CHECK("acsp" == sig(p3, 36));
            FTK_CHECK(10 == u32(p3, 128));
            FTK_CHECK(near(xyz(p3, "rXYZ"), { .515121, .241196, -.001053 }));
            FTK_CHECK(near(xyz(p3, "gXYZ"), { .291977, .692245, .041885 }));
            FTK_CHECK(near(xyz(p3, "bXYZ"), { .157104, .066574, .784073 }));
            FTK_CHECK(near(xyz(p3, "wtpt"), { .9642, 1.0, .8249 }));
            const size_t curve = find(p3, "rTRC");
            FTK_CHECK(curve && "para" == sig(p3, curve));
            FTK_CHECK(find(p3, "gTRC") == curve && find(p3, "bTRC") == curve);

            // sRGB, as the chromaticities a sequence is given; the same
            // as the named primaries.
            const auto srgb = getICCProfile({
                { "Chromaticities", "0.64 0.33 0.3 0.6 0.15 0.06 0.3127 0.329" },
                { "Color Transfer", "iec61966-2-1" } });
            FTK_CHECK(near(xyz(srgb, "rXYZ"), { .436066, .222488, .013916 }));
            FTK_CHECK(near(xyz(srgb, "gXYZ"), { .385147, .716873, .097076 }));
            FTK_CHECK(near(xyz(srgb, "bXYZ"), { .143066, .060608, .714096 }));
            FTK_CHECK(srgb == getICCProfile({
                { "Color Primaries", "bt709" },
                { "Color Transfer", "iec61966-2-1" } }));

            // Rec. 1886's gamma: a curve of the one parameter.
            const auto rec709 = getICCProfile({
                { "Color Primaries", "bt709" },
                { "Color Transfer", "bt709" } });
            const size_t gamma = find(rec709, "rTRC");
            FTK_CHECK(gamma && 0 == (u32(rec709, gamma + 8) >> 16));
            FTK_CHECK(std::abs(static_cast<int32_t>(u32(rec709, gamma + 12)) / 65536.0 - 2.4) < .0001);
        }
    }
}
