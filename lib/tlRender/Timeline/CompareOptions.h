// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/Video.h>

#include <ftk/Core/Box.h>

namespace tl
{
    //! Comparison modes.
    enum class TL_TIMELINE_API_TYPE Compare
    {
        //! Not comparing: the A file on its own. The comparisons are a set of
        //! toggles in the user interface, and this is the state with none of
        //! them on, so it is not offered as one of them.
        None,
        B,
        Wipe,

        //! The same half of each file, one of them mirrored, so that the two
        //! meet at the middle on the same picture. What a side by side
        //! comparison cannot do is put the same thing next to itself, which
        //! is what judging a colour adjustment needs.
        Butterfly,

        Overlay,
        Difference,
        Horizontal,
        Vertical,
        Tile,

        Count,
        First = None
    };
    FTK_ENUM(TL_TIMELINE_API, Compare);

    //! Comparison time modes.
    enum class TL_TIMELINE_API_TYPE CompareTime
    {
        Relative,
        Absolute,

        Count,
        First = Relative
    };
    FTK_ENUM(TL_TIMELINE_API, CompareTime);

    //! Comparison options.
    struct TL_TIMELINE_API_TYPE CompareOptions
    {
        Compare  compare      = Compare::None;
        ftk::V2F wipeCenter   = ftk::V2F(.5F, .5F);
        float    wipeRotation = 0.F;
        float    overlay      = .5F;

        //! What the difference is multiplied by before it is shown. The
        //! differences worth looking for are often a code value or two --
        //! what a compressed version differs from its source by -- and at
        //! their own size they are indistinguishable from black.
        float    differenceGain = 1.F;

        bool     sameSize     = true;

        TL_TIMELINE_API bool operator == (const CompareOptions&) const;
        TL_TIMELINE_API bool operator != (const CompareOptions&) const;
    };

    //! Get the bounds for the given compare mode.
    TL_TIMELINE_API std::vector<ftk::Box2I> getBounds(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the boxes for the given compare mode.
    TL_TIMELINE_API std::vector<ftk::Box2I> getBoxes(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the boxes for the given compare mode.
    TL_TIMELINE_API std::vector<ftk::Box2I> getBoxes(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<VideoFrame>&);

    //! Get the render size for the given compare mode.
    TL_TIMELINE_API ftk::Size2I getRenderSize(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<ftk::ImageInfo>&);

    //! Get the render size for the given compare mode.
    TL_TIMELINE_API ftk::Size2I getRenderSize(
        const CompareOptions&,
        const AspectRatioOptions&,
        const std::vector<VideoFrame>&);

    //! Get a compare time.
    TL_TIMELINE_API OTIO_NS::RationalTime getCompareTime(
        const OTIO_NS::RationalTime& sourceTime,
        const OTIO_NS::TimeRange& sourceTimeRange,
        const OTIO_NS::TimeRange& compareTimeRange,
        CompareTime);

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const CompareOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, CompareOptions&);

    ///@}
}
