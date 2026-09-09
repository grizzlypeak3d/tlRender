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
        FTK_ENUM_IMPL(
            InOutDisplay,
            "Inside Range",
            "Outside Range");

        FTK_ENUM_IMPL(
            CacheDisplay,
            "Video And Audio",
            "Video Only");

        FTK_ENUM_IMPL(
            WaveformPrim,
            "Mesh",
            "Image");

        bool Marker::operator == (const Marker& other) const
        {
            return
                name == other.name &&
                color == other.color &&
                tl::compareExact(range, other.range);
        }

        bool Marker::operator != (const Marker& other) const
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

#if TLRENDER_OTIO_ITEM_COLOR
        ftk::Color4F toColor(const OTIO_NS::Color& value)
        {
            return ftk::Color4F(
                static_cast<float>(value.r()),
                static_cast<float>(value.g()),
                static_cast<float>(value.b()),
                static_cast<float>(value.a()));
        }
#endif // TLRENDER_OTIO_ITEM_COLOR

#if TLRENDER_OTIO_MARKER_COLOR
        ftk::Color4F getMarkerColor(const std::optional<OTIO_NS::Color>& value)
        {
            // OTIO gives markers green by default, so markers that do not
            // carry a color of their own keep the appearance they had.
            return value.has_value() ?
                toColor(value.value()) :
                ftk::Color4F(0.F, 1.F, 0.F);
        }
#else // TLRENDER_OTIO_MARKER_COLOR
        ftk::Color4F getMarkerColor(const std::string& value)
        {
            // The named colors of older OTIO markers, green being the
            // default there too.
            static const std::map<std::string, ftk::Color4F> colors =
            {
                { "PINK",    ftk::Color4F(1.F, .75F, .8F) },
                { "RED",     ftk::Color4F(1.F, 0.F, 0.F) },
                { "ORANGE",  ftk::Color4F(1.F, .65F, 0.F) },
                { "YELLOW",  ftk::Color4F(1.F, 1.F, 0.F) },
                { "GREEN",   ftk::Color4F(0.F, 1.F, 0.F) },
                { "CYAN",    ftk::Color4F(0.F, 1.F, 1.F) },
                { "BLUE",    ftk::Color4F(0.F, 0.F, 1.F) },
                { "PURPLE",  ftk::Color4F(.5F, 0.F, .5F) },
                { "MAGENTA", ftk::Color4F(1.F, 0.F, 1.F) },
                { "BLACK",   ftk::Color4F(0.F, 0.F, 0.F) },
                { "WHITE",   ftk::Color4F(1.F, 1.F, 1.F) }
            };
            const auto i = colors.find(value);
            return i != colors.end() ? i->second : ftk::Color4F(0.F, 1.F, 0.F);
        }
#endif // TLRENDER_OTIO_MARKER_COLOR

        ftk::Color4F getItemColor(
            const OTIO_NS::Item* otioItem,
            const ftk::Color4F& defaultColor,
            const DisplayOptions& displayOptions)
        {
            ftk::Color4F out = defaultColor;
#if TLRENDER_OTIO_ITEM_COLOR
            if (displayOptions.clipColors && otioItem)
            {
                if (const auto color = otioItem->color())
                {
                    out = toColor(color.value());
                }
            }
#endif // TLRENDER_OTIO_ITEM_COLOR
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
