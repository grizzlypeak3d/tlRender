// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/Video.h>

#include <ftk/Core/Box.h>

namespace tl
{
    //! Comparison modes.
    enum class TL_API_TYPE Compare
    {
        A,
        B,
        Wipe,
        Overlay,
        Difference,
        Horizontal,
        Vertical,
        Tile,

        Count,
        First = A
    };
    TL_ENUM(Compare);

    //! Comparison time modes.
    enum class TL_API_TYPE CompareTime
    {
        Relative,
        Absolute,

        Count,
        First = Relative
    };
    TL_ENUM(CompareTime);

    //! Comparison options.
    struct TL_API_TYPE CompareOptions
    {
        Compare  compare      = Compare::A;
        ftk::V2F wipeCenter   = ftk::V2F(.5F, .5F);
        float    wipeRotation = 0.F;
        float    overlay      = .5F;
        bool     fitToA       = true;
        bool     tileZoomSync = true;
        std::vector<double> tileZooms;

        TL_API bool operator == (const CompareOptions&) const;
        TL_API bool operator != (const CompareOptions&) const;
    };

    //! Get the zoom for a tile. Missing entries use the neutral zoom.
    TL_API double getTileZoom(const CompareOptions&, size_t);

    //! Scale a box around the center of its tile.
    TL_API ftk::Box2I getTileZoomBox(
        const ftk::Box2I&,
        const ftk::Box2I& tile,
        double zoom);

    //! Get the bounds for the given compare mode.
    TL_API std::vector<ftk::Box2I> getBounds(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the bounds for the given compare mode.
    TL_API std::vector<ftk::Box2I> getBounds(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<VideoFrame>&);

    //! Get the boxes for the given compare mode.
    TL_API std::vector<ftk::Box2I> getBoxes(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the boxes for the given compare mode.
    TL_API std::vector<ftk::Box2I> getBoxes(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<VideoFrame>&);

    //! Get the render size for the given compare mode.
    TL_API ftk::Size2I getRenderSize(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the render size for the given compare mode.
    TL_API ftk::Size2I getRenderSize(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<VideoFrame>&);

    //! Get a compare time.
    TL_API OTIO_NS::RationalTime getCompareTime(
        const OTIO_NS::RationalTime& sourceTime,
        const OTIO_NS::TimeRange& sourceTimeRange,
        const OTIO_NS::TimeRange& compareTimeRange,
        CompareTime);

    //! \name Serialize
    ///@{

    TL_API void to_json(nlohmann::json&, const CompareOptions&);

    TL_API void from_json(const nlohmann::json&, CompareOptions&);

    ///@}
}
