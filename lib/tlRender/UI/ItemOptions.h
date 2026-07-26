// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/TimeUnits.h>
#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/Color.h>
#include <ftk/Core/Util.h>

#include <opentimelineio/item.h>

namespace tl
{
    namespace ui
    {
        //! Item data.
        struct TL_API_TYPE ItemData
        {
            double speed = 0.0;
            std::string dir;
            Options options;
            std::shared_ptr<ITimeUnitsModel> timeUnitsModel;
        };

        //! In/out points display options.
        enum class TL_API_TYPE InOutDisplay
        {
            InsideRange,
            OutsideRange,

            Count,
            First = InsideRange
        };
        TL_ENUM(InOutDisplay);
        
        //! Cache display options.
        enum class TL_API_TYPE CacheDisplay
        {
            VideoAndAudio,
            VideoOnly,

            Count,
            First = VideoAndAudio
        };
        TL_ENUM(CacheDisplay);

        //! Waveform primitive type.
        enum class TL_API_TYPE WaveformPrim
        {
            Mesh,
            Image,

            Count,
            First = Mesh
        };
        TL_ENUM(WaveformPrim);

        //! Item options.
        struct TL_API_TYPE ItemOptions
        {
            bool inputEnabled = true;

            TL_API bool operator == (const ItemOptions&) const;
            TL_API bool operator != (const ItemOptions&) const;
        };

        //! Display options.
        struct TL_API_TYPE DisplayOptions
        {
            InOutDisplay inOutDisplay = InOutDisplay::InsideRange;
            CacheDisplay cacheDisplay = CacheDisplay::VideoAndAudio;

            bool minimize = true;

            //! Color items using the color the OTIO item carries, where it
            //! has one. Items without a color are unaffected.
            bool clipColors = true;

            bool thumbnails = true;
            int thumbnailHeight = 100;
            bool waveforms = true;
            int waveformWidth = 50;
            int waveformHeight = 50;
            WaveformPrim waveformPrim = WaveformPrim::Mesh;

            float clipRectScale = 2.F;

            OCIOOptions ocio;
            LUTOptions lut;

            TL_API bool operator == (const DisplayOptions&) const;
            TL_API bool operator != (const DisplayOptions&) const;
        };

        //! Marker.
        struct TL_API_TYPE Marker
        {
            std::string name;
            ftk::Color4F color;
            OTIO_NS::TimeRange range;
        };

        //! Get the markers from an item.
        TL_API std::vector<Marker> getMarkers(const OTIO_NS::Item*);

        //! Convert an OTIO color.
        //!
        //! The components are sRGB encoded and range from zero to one, which
        //! is what the user interface works in, so they are used as they are.
        TL_API ftk::Color4F toColor(const OTIO_NS::Color&);

        //! Convert a marker color, which is optional; markers without one are
        //! given the color OTIO uses by default.
        TL_API ftk::Color4F getMarkerColor(const std::optional<OTIO_NS::Color>&);

        //! Get the color for an item, which is the color the OTIO item carries
        //! where it has one, and the given default otherwise.
        TL_API ftk::Color4F getItemColor(
            const OTIO_NS::Item*,
            const ftk::Color4F& defaultColor,
            const DisplayOptions&);

        //! \name Serialize
        ///@{

        TL_API void to_json(nlohmann::json&, const ItemOptions&);
        TL_API void to_json(nlohmann::json&, const DisplayOptions&);

        TL_API void from_json(const nlohmann::json&, ItemOptions&);
        TL_API void from_json(const nlohmann::json&, DisplayOptions&);

        ///@}
    }
}
