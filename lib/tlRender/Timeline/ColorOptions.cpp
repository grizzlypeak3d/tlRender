// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/ColorOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorTransforms.h>
#endif // TLRENDER_OCIO

#include <algorithm>
#include <array>
#include <sstream>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    FTK_ENUM_IMPL(
        OCIOConfig,
        "Built In",
        "Environment Variable",
        "File");

    bool OCIOOptions::operator == (const OCIOOptions& other) const
    {
        return
            enabled == other.enabled &&
            config == other.config &&
            fileName == other.fileName &&
            input == other.input &&
            display == other.display &&
            view == other.view &&
            look == other.look;
    }

    bool OCIOOptions::operator != (const OCIOOptions& other) const
    {
        return !(*this == other);
    }

    ftk::ImageTags getDisplayColorTags(
        const OCIOOptions& options,
        bool sequence)
    {
        ftk::ImageTags out;
        if (options.enabled &&
            !options.display.empty() &&
            !options.view.empty())
        {
            std::string primaries;
            std::string transfer;
            std::string chromaticities;
            const std::string rec709 =
                "0.64 0.33 0.3 0.6 0.15 0.06 0.3127 0.329";
            const std::string rec2020 =
                "0.708 0.292 0.17 0.797 0.131 0.046 0.3127 0.329";
            if ("sRGB - Display" == options.display)
            {
                primaries = "bt709";
                transfer = "iec61966-2-1";
                chromaticities = rec709;
            }
            else if ("Rec.1886 Rec.709 - Display" == options.display)
            {
                primaries = "bt709";
                transfer = "bt709";
                chromaticities = rec709;
            }
            else if ("Rec.2100-PQ - Display" == options.display)
            {
                primaries = "bt2020";
                transfer = "smpte2084";
                chromaticities = rec2020;
            }
            else if ("Rec.2100-HLG - Display" == options.display)
            {
                primaries = "bt2020";
                transfer = "arib-std-b67";
                chromaticities = rec2020;
            }
            if (sequence)
            {
                if (!chromaticities.empty())
                {
                    out["Chromaticities"] = chromaticities;
                }
            }
            else if (!primaries.empty())
            {
                out["Color Primaries"] = primaries;
                out["Color Transfer"] = transfer;
            }
        }
        return out;
    }

    FTK_ENUM_IMPL(
        LUTOrder,
        "Post-Config",
        "Pre-Config");

    bool LUTOptions::operator == (const LUTOptions& other) const
    {
        return
            enabled == other.enabled &&
            fileName == other.fileName &&
            order == other.order;
    }

    bool LUTOptions::operator != (const LUTOptions& other) const
    {
        return !(*this == other);
    }

    std::vector<std::string> getLUTFormatNames()
    {
        std::vector<std::string> out;
#if defined(TLRENDER_OCIO)
        for (int i = 0; i < OCIO::FileTransform::GetNumFormats(); ++i)
        {
            out.push_back(OCIO::FileTransform::GetFormatNameByIndex(i));
        }
#endif // TLRENDER_OCIO
        return out;
    }

    std::vector<std::string> getLUTFormatExts()
    {
        std::vector<std::string> out;
#if defined(TLRENDER_OCIO)
        for (int i = 0; i < OCIO::FileTransform::GetNumFormats(); ++i)
        {
            std::string extension = OCIO::FileTransform::GetFormatExtensionByIndex(i);
            if (!extension.empty() && extension[0] != '.')
            {
                extension.insert(extension.begin(), '.');
            }
            out.push_back(extension);
        }
#endif // TLRENDER_OCIO
        return out;
    }

    void to_json(nlohmann::json& json, const OCIOOptions& value)
    {
        json["Enabled"] = value.enabled;
        json["Config"] = to_string(value.config);
        json["FileName"] = value.fileName;
        json["Input"] = value.input;
        json["Display"] = value.display;
        json["View"] = value.view;
        json["Look"] = value.look;
    }

    void to_json(nlohmann::json& json, const LUTOptions& value)
    {
        json["Enabled"] = value.enabled;
        json["FileName"] = value.fileName;
        json["Order"] = to_string(value.order);
    }

    void from_json(const nlohmann::json& json, OCIOOptions& value)
    {
        json.at("Enabled").get_to(value.enabled);
        from_string(json.at("Config").get<std::string>(), value.config);
        json.at("FileName").get_to(value.fileName);
        json.at("Input").get_to(value.input);
        json.at("Display").get_to(value.display);
        json.at("View").get_to(value.view);
        json.at("Look").get_to(value.look);
    }

    void from_json(const nlohmann::json& json, LUTOptions& value)
    {
        json.at("Enabled").get_to(value.enabled);
        json.at("FileName").get_to(value.fileName);
        from_string(json.at("Order").get<std::string>(), value.order);
    }
}
