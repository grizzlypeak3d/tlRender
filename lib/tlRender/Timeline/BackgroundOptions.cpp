// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/BackgroundOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <sstream>

namespace tl
{
    FTK_ENUM_IMPL(
        Background,
        "Solid",
        "Checkers",
        "Gradient");

    bool Outline::operator == (const Outline& other) const
    {
        return
            enabled == other.enabled &&
            width == other.width &&
            color == other.color;
    }

    bool Outline::operator != (const Outline& other) const
    {
        return !(*this == other);
    }

    bool BackgroundOptions::operator == (const BackgroundOptions& other) const
    {
        return
            type == other.type &&
            solidColor == other.solidColor &&
            checkersColor == other.checkersColor &&
            checkersSize == other.checkersSize &&
            gradientColor == other.gradientColor &&
            outline == other.outline;
    }

    bool BackgroundOptions::operator != (const BackgroundOptions& other) const
    {
        return !(*this == other);
    }

    void to_json(nlohmann::json& json, const Outline& in)
    {
        json["Enabled"] = in.enabled;
        json["Width"] = in.width;
        json["Color"] = in.color;
    }

    void to_json(nlohmann::json& json, const BackgroundOptions& in)
    {
        json["Type"] = to_string(in.type);
        json["SolidColor"] = in.solidColor;
        json["CheckersColor"].push_back(in.checkersColor.first);
        json["CheckersColor"].push_back(in.checkersColor.second);
        json["CheckersSize"] = in.checkersSize;
        json["GradientColor"].push_back(in.gradientColor.first);
        json["GradientColor"].push_back(in.gradientColor.second);
        json["Outline"] = in.outline;
    }

    void from_json(const nlohmann::json& json, Outline& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Width").get_to(out.width);
        json.at("Color").get_to(out.color);
    }

    void from_json(const nlohmann::json& json, BackgroundOptions& out)
    {
        from_string(json.at("Type").get<std::string>(), out.type);
        json.at("SolidColor").get_to(out.solidColor);
        json.at("CheckersColor")[0].get_to(out.checkersColor.first);
        json.at("CheckersColor")[1].get_to(out.checkersColor.second);
        json.at("CheckersSize").get_to(out.checkersSize);
        json.at("GradientColor")[0].get_to(out.gradientColor.first);
        json.at("GradientColor")[1].get_to(out.gradientColor.second);
        json.at("Outline").get_to(out.outline);
    }
}
