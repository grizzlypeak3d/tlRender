// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/ColorOptionsTest.h>

#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/RenderPrivate.h>

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
                // The order every renderer and the thumbnails go by.
                FTK_CHECK(getColorStages(LUTOrder::PostConfig) == std::vector<ColorStage>({
                    ColorStage::ToLinear,
                    ColorStage::Corrections,
                    ColorStage::Display,
                    ColorStage::LUT }));
                FTK_CHECK(getColorStages(LUTOrder::PreConfig) == std::vector<ColorStage>({
                    ColorStage::LUT,
                    ColorStage::ToLinear,
                    ColorStage::Corrections,
                    ColorStage::Display }));

                ColorStageCalls calls = getColorStageCalls(
                    LUTOrder::PostConfig, "lut();", "toLinear();", "display();", "  ");
                FTK_CHECK("toLinear();" == calls.before);
                FTK_CHECK("display();\n  lut();" == calls.after);
                calls = getColorStageCalls(
                    LUTOrder::PreConfig, "lut();", "toLinear();", "display();", "  ");
                FTK_CHECK("lut();\n  toLinear();" == calls.before);
                FTK_CHECK("display();" == calls.after);
                // A stage that is not there is left out.
                calls = getColorStageCalls(LUTOrder::PreConfig, "", "", "display();");
                FTK_CHECK(calls.before.empty());
                FTK_CHECK("display();" == calls.after);
            }
#if defined(TLRENDER_OCIO)
            {
                // Built once for each set of options and input; a failure is
                // remembered and told once.
                struct Data
                {
                    std::string input;
                };
                OCIOCache<Data> cache;
                int built = 0;
                int errors = 0;
                const auto init = [&built](Data& data, const OCIOOptions& options)
                {
                    ++built;
                    if ("bad" == options.input)
                    {
                        throw std::runtime_error("bad input");
                    }
                    data.input = options.input;
                };
                const auto error = [&errors](const std::string&) { ++errors; };
                OCIOOptions options;
                options.enabled = true;
                options.display = "Display";
                options.view = "View";
                FTK_CHECK(cache.setOptions(options).empty());
                FTK_CHECK(getOCIOOptionsKey(options) == cache.getKey());
                auto a = cache.get("A", init, error);
                FTK_CHECK(a && "A" == a->input);
                FTK_CHECK(cache.get("A", init, error) == a);
                FTK_CHECK(1 == built);
                FTK_CHECK(!cache.get("bad", init, error));
                FTK_CHECK(!cache.get("bad", init, error));
                FTK_CHECK(2 == built);
                FTK_CHECK(1 == errors);

                // The last few sets of options are kept; those let go of are
                // said, for the renderer's shaders. The options the cache
                // starts with count as a set.
                const std::string defaultKey = getOCIOOptionsKey(OCIOOptions());
                const std::string firstKey = cache.getKey();
                std::vector<std::string> erased;
                for (int i = 0; i < 4; ++i)
                {
                    OCIOOptions other = options;
                    other.view = "View " + std::to_string(i);
                    const auto keys = cache.setOptions(other);
                    erased.insert(erased.end(), keys.begin(), keys.end());
                }
                FTK_CHECK(erased == std::vector<std::string>({ defaultKey, firstKey }));
                FTK_CHECK(cache.setOptions(options).size() == 1);
                FTK_CHECK(cache.get("A", init, error) != a);
                FTK_CHECK(3 == built);

                // A layer's input: its own, else the resolver's, kept by
                // path; the item's where neither says.
                int resolved = 0;
                cache.setInputResolver(
                    [&resolved](const std::string& path, const ftk::ImageTags&)
                    {
                        ++resolved;
                        return "exr" == path ? std::string("ACEScg") : std::string();
                    });
                FTK_CHECK("Own" == cache.getLayerInput("Own", "exr", nullptr));
                FTK_CHECK("ACEScg" == cache.getLayerInput("", "exr", nullptr));
                FTK_CHECK("ACEScg" == cache.getLayerInput("", "exr", nullptr));
                FTK_CHECK(1 == resolved);
                VideoFrame frame;
                VideoLayer layer;
                layer.path = "exr";
                layer.pathB = "png";
                frame.layers.push_back(layer);
                auto inputs = cache.getLayerInputs(frame, "Item");
                FTK_CHECK(1 == inputs.size());
                FTK_CHECK("ACEScg" == inputs[0].first);
                FTK_CHECK("Item" == inputs[0].second);
                FTK_CHECK("ACEScg" == getFirstLayerInput(inputs));
                FTK_CHECK(getFirstLayerInput({}).empty());
                OCIOOptions off = options;
                off.enabled = false;
                cache.setOptions(off);
                FTK_CHECK(cache.getLayerInputs(frame, "Item").empty());
            }
#endif // TLRENDER_OCIO
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
