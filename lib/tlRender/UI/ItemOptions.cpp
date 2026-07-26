// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/ItemOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <opentimelineio/marker.h>

#include <sstream>

namespace tl
{
    namespace ui
    {
        TL_ENUM_IMPL(
            InOutDisplay,
            "Inside Range",
            "Outside Range");

        TL_ENUM_IMPL(
            CacheDisplay,
            "Video And Audio",
            "Video Only");

        TL_ENUM_IMPL(
            WaveformPrim,
            "Mesh",
            "Image");

        bool ItemOptions::operator == (const ItemOptions& other) const
        {
            return
                inputEnabled == other.inputEnabled;
        }

        bool ItemOptions::operator != (const ItemOptions& other) const
        {
            return !(*this == other);
        }

        bool DisplayOptions::operator == (const DisplayOptions& other) const
        {
            return
                inOutDisplay == other.inOutDisplay &&
                cacheDisplay == other.cacheDisplay &&
                minimize == other.minimize &&
                clipColors == other.clipColors &&
                thumbnails == other.thumbnails &&
                thumbnailHeight == other.thumbnailHeight &&
                waveforms == other.waveforms &&
                waveformWidth == other.waveformWidth &&
                waveformHeight == other.waveformHeight &&
                waveformPrim == other.waveformPrim &&
                clipRectScale == other.clipRectScale &&
                ocio == other.ocio &&
                lut == other.lut;
        }

        bool DisplayOptions::operator != (const DisplayOptions& other) const
        {
            return !(*this == other);
        }

        std::vector<Marker> getMarkers(const OTIO_NS::Item* item)
        {
            std::vector<Marker> out;
            for (const auto& marker : item->markers())
            {
                out.push_back({
                    marker->name(),
                    getMarkerColor(marker->color()),
                    marker->marked_range() });
            }
            return out;
        }

        ftk::Color4F toColor(const OTIO_NS::Color& value)
        {
            return ftk::Color4F(
                static_cast<float>(value.r()),
                static_cast<float>(value.g()),
                static_cast<float>(value.b()),
                static_cast<float>(value.a()));
        }

        ftk::Color4F getMarkerColor(const std::optional<OTIO_NS::Color>& value)
        {
            // OTIO gives markers green by default, so markers that do not
            // carry a color of their own keep the appearance they had.
            return value.has_value() ?
                toColor(value.value()) :
                ftk::Color4F(0.F, 1.F, 0.F);
        }

        ftk::Color4F getItemColor(
            const OTIO_NS::Item* otioItem,
            const ftk::Color4F& defaultColor,
            const DisplayOptions& displayOptions)
        {
            ftk::Color4F out = defaultColor;
            if (displayOptions.clipColors && otioItem)
            {
                if (const auto color = otioItem->color())
                {
                    out = toColor(color.value());
                }
            }
            return out;
        }

        void to_json(nlohmann::json& json, const ItemOptions& value)
        {
            json["InputEnabled"] = value.inputEnabled;
        }

        void to_json(nlohmann::json& json, const DisplayOptions& value)
        {
            json["InOutDisplay"] = to_string(value.inOutDisplay);
            json["CacheDisplay"] = to_string(value.cacheDisplay);
            json["Minimize"] = value.minimize;
            json["Thumbnails"] = value.thumbnails;
            json["ThumbnailHeight"] = value.thumbnailHeight;
            json["Waveforms"] = value.waveforms;
            json["WaveformWidth"] = value.waveformWidth;
            json["WaveformHeight"] = value.waveformHeight;
            json["WaveformPrim"] = to_string(value.waveformPrim);
            json["ClipRectScale"] = value.clipRectScale;
            json["OCIO"] = value.ocio;
            json["LUT"] = value.lut;
        }

        void from_json(const nlohmann::json& json, ItemOptions& value)
        {
            json.at("InputEnabled").get_to(value.inputEnabled);
        }

        void from_json(const nlohmann::json& json, DisplayOptions& value)
        {
            from_string(json["InOutDisplay"].get<std::string>(), value.inOutDisplay);
            from_string(json["CacheDisplay"].get<std::string>(), value.cacheDisplay);
            json["Minimize"].get_to(value.minimize);
            json["Thumbnails"].get_to(value.thumbnails);
            json["ThumbnailHeight"].get_to(value.thumbnailHeight);
            json["Waveforms"].get_to(value.waveforms);
            json["WaveformWidth"].get_to(value.waveformWidth);
            json["WaveformHeight"].get_to(value.waveformHeight);
            from_string(json["WaveformPrim"].get<std::string>(), value.waveformPrim);
            json["ClipRectScale"].get_to(value.clipRectScale);
            json["OCIO"].get_to(value.ocio);
            json["LUT"].get_to(value.lut);
        }
    }
}
