// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/ForegroundOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <sstream>

namespace tl
{
    TL_ENUM_IMPL(
        GridCellMode,
        "Cell Size",
        "Cell Count");

    TL_ENUM_IMPL(
        GridLabels,
        "None",
        "Pixels",
        "Alphanumeric");

    namespace
    {
        std::string alpha(int value)
        {
            std::string out;
            if (0 == value)
            {
                out = "A";
            }
            else
            {
                while (value > 0)
                {
                    const int remainder = value % 26;
                    out += 'A' + remainder;
                    value /= 26;
                }
                std::reverse(out.begin(), out.end());
            }
            return out;
        }
    }

    std::string getLabel(GridLabels labels, int x, int y)
    {
        std::stringstream ss;
        switch (labels)
        {
        case GridLabels::Pixels:
            ss << x << " " << y;
            break;
        case GridLabels::Alphanumeric:
            ss << alpha(x) << " " << y;
            break;
        default: break;
        }
        return ss.str();
    }

    bool Grid::operator == (const Grid& other) const
    {
        return
            enabled == other.enabled &&
            cellMode == other.cellMode &&
            cellSize == other.cellSize &&
            cellCount == other.cellCount &&
            lineWidth == other.lineWidth &&
            color == other.color &&
            labels == other.labels &&
            textColor == other.textColor &&
            overlayColor == other.overlayColor &&
            fontInfo == other.fontInfo &&
            textMargin == other.textMargin;
    }

    bool Grid::operator != (const Grid& other) const
    {
        return !(*this == other);
    }

    bool CenterMarker::operator == (const CenterMarker& other) const
    {
        return
            enabled == other.enabled &&
            size == other.size &&
            width == other.width &&
            color == other.color;
    }

    bool CenterMarker::operator != (const CenterMarker& other) const
    {
        return !(*this == other);
    }

    bool MissingIndicator::operator == (const MissingIndicator& other) const
    {
        return
            enabled == other.enabled &&
            width == other.width &&
            color == other.color;
    }

    bool MissingIndicator::operator != (const MissingIndicator& other) const
    {
        return !(*this == other);
    }

    bool ForegroundOptions::operator == (const ForegroundOptions& other) const
    {
        return
            grid == other.grid &&
            centerMarker == other.centerMarker &&
            missingIndicator == other.missingIndicator;
    }

    bool ForegroundOptions::operator != (const ForegroundOptions& other) const
    {
        return !(*this == other);
    }

    void to_json(nlohmann::json& json, const Grid& in)
    {
        json["Enabled"] = in.enabled;
        json["CellMode"] = to_string(in.cellMode);
        json["CellSize"] = in.cellSize;
        json["CellCount"] = in.cellCount;
        json["LineWidth"] = in.lineWidth;
        json["Color"] = in.color;
        json["Labels"] = to_string(in.labels);
        json["TextColor"] = in.textColor;
        json["OverlayColor"] = in.overlayColor;
        json["FontInfo"] = in.fontInfo;
        json["TextMargin"] = in.textMargin;
    }

    void to_json(nlohmann::json& json, const CenterMarker& in)
    {
        json["Enabled"] = in.enabled;
        json["Size"] = in.size;
        json["Width"] = in.width;
        json["Color"] = in.color;
    }

    void to_json(nlohmann::json& json, const MissingIndicator& in)
    {
        json["Enabled"] = in.enabled;
        json["Width"] = in.width;
        json["Color"] = in.color;
    }

    void to_json(nlohmann::json& json, const ForegroundOptions& in)
    {
        json["Grid"] = in.grid;
        json["CenterMarker"] = in.centerMarker;
        json["MissingIndicator"] = in.missingIndicator;
    }

    void from_json(const nlohmann::json& json, Grid& out)
    {
        json.at("Enabled").get_to(out.enabled);
        from_string(json.at("CellMode").get<std::string>(), out.cellMode);
        json.at("CellSize").get_to(out.cellSize);
        json.at("CellCount").get_to(out.cellCount);
        json.at("LineWidth").get_to(out.lineWidth);
        json.at("Color").get_to(out.color);
        from_string(json.at("Labels").get<std::string>(), out.labels);
        json.at("TextColor").get_to(out.textColor);
        json.at("OverlayColor").get_to(out.overlayColor);
        json.at("FontInfo").get_to(out.fontInfo);
        json.at("TextMargin").get_to(out.textMargin);
    }

    void from_json(const nlohmann::json& json, CenterMarker& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Size").get_to(out.size);
        json.at("Width").get_to(out.width);
        json.at("Color").get_to(out.color);
    }

    void from_json(const nlohmann::json& json, MissingIndicator& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Width").get_to(out.width);
        json.at("Color").get_to(out.color);
    }

    void from_json(const nlohmann::json& json, ForegroundOptions& out)
    {
        json.at("Grid").get_to(out.grid);
        json.at("CenterMarker").get_to(out.centerMarker);
        // Added later, so a settings file written before it exists is still
        // read rather than throwing and losing the rest of the options.
        if (json.contains("MissingIndicator"))
        {
            json.at("MissingIndicator").get_to(out.missingIndicator);
        }
    }
}
