// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
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
        struct TL_UI_API_TYPE ItemData
        {
            double speed = 0.0;
            std::string dir;
            Options options;
            std::shared_ptr<ITimeUnitsModel> timeUnitsModel;

            //! How to turn a time on the timeline into the time to show for
            //! it, which are not always the same: a sequence read with frames
            //! left out is played at the length of the frames it has, so
            //! counting from the start does not name the frame. Left unset the
            //! two are the same. Only positions go through this; a duration is
            //! a length either way.
            std::function<
                OTIO_NS::RationalTime(const OTIO_NS::RationalTime&)> toMediaTime;
        };

        //! In/out points display options.
        enum class TL_UI_API_TYPE InOutDisplay
        {
            InsideRange,
            OutsideRange,

            Count,
            First = InsideRange
        };
        FTK_ENUM(TL_UI_API, InOutDisplay);
        
        //! Cache display options.
        enum class TL_UI_API_TYPE CacheDisplay
        {
            VideoAndAudio,
            VideoOnly,

            Count,
            First = VideoAndAudio
        };
        FTK_ENUM(TL_UI_API, CacheDisplay);

        //! Waveform primitive type.
        enum class TL_UI_API_TYPE WaveformPrim
        {
            Mesh,
            Image,

            Count,
            First = Mesh
        };
        FTK_ENUM(TL_UI_API, WaveformPrim);

        //! Item options.
        struct TL_UI_API_TYPE ItemOptions
        {
            bool inputEnabled = true;

            TL_UI_API bool operator == (const ItemOptions&) const;
            TL_UI_API bool operator != (const ItemOptions&) const;
        };

        //! Colors for timeline items, keyed by track and then by where the
        //! item starts within that track. Both are needed: tracks run over
        //! the same times, so a start time on its own would color a video
        //! clip and whatever the audio track happens to have beside it.
        //! Drawn as an outline around the item rather than as its color.
        typedef std::map<int, std::map<OTIO_NS::RationalTime, ftk::Color4F> > ItemColors;

        //! Display options.
        struct TL_UI_API_TYPE DisplayOptions
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

            TL_UI_API bool operator == (const DisplayOptions&) const;
            TL_UI_API bool operator != (const DisplayOptions&) const;
        };

        //! Marker.
        struct TL_UI_API_TYPE Marker
        {
            std::string name;
            ftk::Color4F color;
            OTIO_NS::TimeRange range;
        };

        //! Get the markers from an item.
        TL_UI_API std::vector<Marker> getMarkers(const OTIO_NS::Item*);

        //! Convert an OTIO color.
        //!
        //! The components are sRGB encoded and range from zero to one, which
        //! is what the user interface works in, so they are used as they are.
        TL_UI_API ftk::Color4F toColor(const OTIO_NS::Color&);

        //! Convert a marker color, which is optional; markers without one are
        //! given the color OTIO uses by default.
        TL_UI_API ftk::Color4F getMarkerColor(const std::optional<OTIO_NS::Color>&);

        //! Get the color for an item, which is the color the OTIO item carries
        //! where it has one, and the given default otherwise.
        TL_UI_API ftk::Color4F getItemColor(
            const OTIO_NS::Item*,
            const ftk::Color4F& defaultColor,
            const DisplayOptions&);

        //! \name Serialize
        ///@{

        TL_UI_API void to_json(nlohmann::json&, const ItemOptions&);
        TL_UI_API void to_json(nlohmann::json&, const DisplayOptions&);

        TL_UI_API void from_json(const nlohmann::json&, ItemOptions&);
        TL_UI_API void from_json(const nlohmann::json&, DisplayOptions&);

        ///@}
    }
}
