// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Color.h>
#include <ftk/Core/FontSystem.h>

namespace tl
{
    //! Grid cell modes.
    enum class TL_TIMELINE_API_TYPE GridCellMode
    {
        CellSize,
        CellCount,

        Count,
        First = CellSize
    };
    TL_ENUM(GridCellMode);

    //! Grid labels.
    enum class TL_TIMELINE_API_TYPE GridLabels
    {
        None,
        Pixels,
        Alphanumeric,

        Count,
        First = None
    };
    TL_ENUM(GridLabels);

    //! Get a grid label.
    TL_TIMELINE_API std::string getLabel(GridLabels, int x, int y);

    //! Grid.
    struct TL_TIMELINE_API_TYPE Grid
    {
        bool          enabled      = false;
        GridCellMode  cellMode     = GridCellMode::CellSize;
        int           cellSize     = 100;
        ftk::V2I      cellCount    = ftk::V2I(5, 3);
        int           lineWidth    = 2;
        ftk::Color4F  color        = ftk::Color4F(0.F, 0.F, 0.F);
        GridLabels    labels       = GridLabels::None;
        ftk::Color4F  textColor    = ftk::Color4F(1.F, 1.F, 1.F);
        ftk::Color4F  overlayColor = ftk::Color4F(0.F, 0.F, 0.F, .5F);
        ftk::FontInfo fontInfo     = ftk::FontInfo(ftk::getDefaultFont(ftk::FontType::Mono), 12);
        int           textMargin   = 2;

        TL_TIMELINE_API bool operator == (const Grid&) const;
        TL_TIMELINE_API bool operator != (const Grid&) const;
    };

    //! Center marker.
    struct TL_TIMELINE_API_TYPE CenterMarker
    {
        bool         enabled = false;
        int          size    = 30;
        int          width   = 3;
        ftk::Color4F color   = ftk::Color4F(1.F, 1.F, 1.F);

        TL_TIMELINE_API bool operator == (const CenterMarker&) const;
        TL_TIMELINE_API bool operator != (const CenterMarker&) const;
    };

    //! Missing frame indicator.
    //!
    //! Marks a picture that stands in for a frame the media does not have, so
    //! a held or blank frame is not taken for the frame that was asked for.
    //!
    //! Drawn as a cross over the image. The other overlays -- the grid, the
    //! centre marker, the outline -- are aids for measuring a picture, and a
    //! mark that reads like one of them says the wrong thing about a frame
    //! that is not the frame asked for. A cross cannot be taken for a guide.
    //!
    //! It covers the part of the image that can be seen rather than the image
    //! itself, so zooming into the middle of the picture cannot hide it, the
    //! way it hides the outline.
    struct TL_TIMELINE_API_TYPE MissingIndicator
    {
        bool         enabled = false;
        int          width   = 6;
        ftk::Color4F color   = ftk::Color4F(1.F, 0.F, 0.F);

        TL_TIMELINE_API bool operator == (const MissingIndicator&) const;
        TL_TIMELINE_API bool operator != (const MissingIndicator&) const;
    };

    //! Foreground options.
    struct TL_TIMELINE_API_TYPE ForegroundOptions
    {
        Grid             grid;
        CenterMarker     centerMarker;
        MissingIndicator missingIndicator;

        TL_TIMELINE_API bool operator == (const ForegroundOptions&) const;
        TL_TIMELINE_API bool operator != (const ForegroundOptions&) const;
    };

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const Grid&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const CenterMarker&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const MissingIndicator&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const ForegroundOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, Grid&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, CenterMarker&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, MissingIndicator&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, ForegroundOptions&);

    ///@}
}
