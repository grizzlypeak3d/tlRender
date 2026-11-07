// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/ColorOptionsTest.h>

#include <tlRender/Timeline/ColorOptions.h>

#include <ftk/Core/Assert.h>

#include <nlohmann/json.hpp>
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
                // What a display's pixels are, named for the files written
                // through it.
                OCIOOptions options;
                options.enabled = true;
                options.view = "View";
                options.display = "sRGB - Display";
                auto tags = getDisplayColorTags(options, false);
                FTK_CHECK("bt709" == tags["Color Primaries"]);
                FTK_CHECK("iec61966-2-1" == tags["Color Transfer"]);
                options.display = "Display P3 - Display";
                tags = getDisplayColorTags(options, false);
                FTK_CHECK("smpte432" == tags["Color Primaries"]);
                FTK_CHECK("iec61966-2-1" == tags["Color Transfer"]);
                tags = getDisplayColorTags(options, true);
                FTK_CHECK("0.68 0.32 0.265 0.69 0.15 0.06 0.3127 0.329" == tags["Chromaticities"]);
                FTK_CHECK("iec61966-2-1" == tags["Color Transfer"]);
                FTK_CHECK(tags.find("Color Primaries") == tags.end());
                // A display not known, or no display transform: nothing.
                options.display = "Somewhere - Display";
                FTK_CHECK(getDisplayColorTags(options, false).empty());
                options.display = "Display P3 - Display";
                options.enabled = false;
                FTK_CHECK(getDisplayColorTags(options, false).empty());
            }
            {
                _print("LUT formats: " + ftk::join(getLUTFormatNames(), ", "));
                _print("LUT format extensions: " + ftk::join(getLUTFormatExts(), ", "));
            }
            {
                // A field at a time, which is what notices one left out of
                // the comparison.
                OCIOOptions v;
                FTK_CHECK(v == OCIOOptions());
                v = OCIOOptions();
                v.enabled = true;
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.config = OCIOConfig::EnvVar;
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.fileName = "fileName";
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.input = "input";
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.display = "display";
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.view = "view";
                FTK_CHECK(v != OCIOOptions());
                v = OCIOOptions();
                v.look = "look";
                FTK_CHECK(v != OCIOOptions());
            }
            {
                LUTOptions v;
                FTK_CHECK(v == LUTOptions());
                v = LUTOptions();
                v.enabled = true;
                FTK_CHECK(v != LUTOptions());
                v = LUTOptions();
                v.fileName = "fileName";
                FTK_CHECK(v != LUTOptions());
                v = LUTOptions();
                v.order = LUTOrder::PreConfig;
                FTK_CHECK(v != LUTOptions());
            }
            {
                OCIOOptions v;
                v.enabled = true;
                v.fileName = "config.ocio";
                v.input = "input";
                v.display = "display";
                v.view = "view";
                v.look = "look";
                nlohmann::json json;
                to_json(json, v);
                OCIOOptions v2;
                from_json(json, v2);
                FTK_CHECK(v == v2);
            }
            {
                LUTOptions v;
                v.enabled = true;
                v.fileName = "lut.cube";
                v.order = LUTOrder::PreConfig;
                nlohmann::json json;
                to_json(json, v);
                LUTOptions v2;
                from_json(json, v2);
                FTK_CHECK(v == v2);
            }
        }
    }
}
