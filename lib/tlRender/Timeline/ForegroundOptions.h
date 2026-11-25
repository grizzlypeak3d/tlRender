// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/DisplayOptions.h>
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
    FTK_ENUM(TL_TIMELINE_API, GridCellMode);

    //! Grid labels.
    enum class TL_TIMELINE_API_TYPE GridLabels
    {
        None,
        Pixels,
        Alphanumeric,

        Count,
        First = None
    };
    FTK_ENUM(TL_TIMELINE_API, GridLabels);

    //! Get a grid label.
    TL_TIMELINE_API std::string getLabel(GridLabels, int x, int y);

    //! Grid.
    struct TL_TIMELINE_API_TYPE Grid
    {
        //! Whether the grid is drawn. It is left out while its cells are no
        //! more than the line width plus ten view pixels across.
        bool          enabled      = false;
        //! Whether the cells are a size (cellSize) or a number (cellCount).
        GridCellMode  cellMode     = GridCellMode::CellSize;
        //! The cell size, in image pixels, for GridCellMode::CellSize.
        int           cellSize     = 100;
        //! The number of columns and rows, for GridCellMode::CellCount.
        ftk::V2I      cellCount    = ftk::V2I(5, 3);
        //! The line width, in view pixels.
        int           lineWidth    = 2;
        //! The line color.
        ftk::Color4F  color        = ftk::Color4F(0.F, 0.F, 0.F);
        GridLabels    labels       = GridLabels::None;
        ftk::Color4F  textColor    = ftk::Color4F(1.F, 1.F, 1.F);
        ftk::Color4F  overlayColor = ftk::Color4F(0.F, 0.F, 0.F, .5F);
        ftk::FontInfo fontInfo     = ftk::FontInfo(ftk::getDefaultFont(ftk::FontType::Mono), 12);
        int           textMargin   = 2;

        bool operator == (const Grid&) const = default;
    };

    //! Center marker.
    struct TL_TIMELINE_API_TYPE CenterMarker
    {
        //! Whether the center marker is drawn.
        bool         enabled = false;
        //! How far the cross reaches from the center, in view pixels. The
        //! arms start a third of the way out, leaving the center clear.
        int          size    = 30;
        //! The line width, in view pixels.
        int          width   = 3;
        //! The marker color.
        ftk::Color4F color   = ftk::Color4F(1.F, 1.F, 1.F);

        bool operator == (const CenterMarker&) const = default;
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
        //! Whether the indicator is drawn.
        bool         enabled = false;
        //! The line width of the cross, in view pixels.
        int          width   = 6;
        //! The cross color.
        ftk::Color4F color   = ftk::Color4F(1.F, 0.F, 0.F);

        bool operator == (const MissingIndicator&) const = default;
    };

    //! What the clipping warning compares with its range.
    enum class TL_TIMELINE_API_TYPE ClippingWarningMode
    {
        AnyChannel,
        AllChannels,
        Luminance,

        Count,
        First = AnyChannel
    };
    FTK_ENUM(TL_TIMELINE_API, ClippingWarningMode);

    //! Clipping warning: a pixel outside the range is covered with a
    //! warning color, red above and magenta below (the false color
    //! convention of the camera makers). The mode says what is outside:
    //! any channel, all three, or the luminance. The range is in
    //! displayed values, so the default marks what the display cannot
    //! show; 16/255 to 235/255 marks the video legal range.
    //!
    //! An overlay rather than a display adjustment so the picked color
    //! is the color under the warning, not the warning.
    struct TL_TIMELINE_API_TYPE ClippingWarning
    {
        //! Whether the clipping warning is drawn.
        bool                enabled = false;
        //! What is compared with the range.
        ClippingWarningMode mode    = ClippingWarningMode::AnyChannel;
        //! The bottom of the range; what is below it is marked magenta.
        float               low     = 0.F;
        //! The top of the range; what is above it is marked red.
        float               high    = 1.F;

        bool operator == (const ClippingWarning&) const = default;
    };

    //! Frame guide: the frame of another aspect ratio within the picture,
    //! 2.39 in a 16:9 picture say, outlined, with the rest masked. The
    //! lines are view pixels wide, so they stay one width however far the
    //! picture is zoomed. The aspect is kept as a ratio, so 16:9 is
    //! exactly that rather than 1.78.
    struct TL_TIMELINE_API_TYPE FrameGuide
    {
        //! Whether the frame guide is drawn.
        bool         enabled     = false;
        //! The aspect ratio of the frame. One that is not valid draws no
        //! guide.
        AspectRatio  aspect      = AspectRatio(2.39F);
        //! The opacity of the black mask outside the frame, from 0 to 1.
        float        maskOpacity = .85F;
        //! The width of the frame's outline, in view pixels, inside the
        //! frame.
        int          lineWidth   = 2;
        //! The outline color.
        ftk::Color4F color       = ftk::Color4F(1.F, 1.F, 1.F, .5F);

        bool operator == (const FrameGuide&) const = default;
    };

    //! Safe areas: the action and title safe areas, as fractions of the
    //! frame -- the frame guide's when it is on, the picture's otherwise.
    struct TL_TIMELINE_API_TYPE SafeAreas
    {
        //! Whether the safe areas are drawn.
        bool         enabled   = false;
        //! The action safe area, as a fraction of the frame's width and
        //! height.
        float        action    = .93F;
        //! The title safe area, as a fraction of the frame's width and
        //! height.
        float        title     = .9F;
        //! The line width, in view pixels.
        int          lineWidth = 2;
        //! The line color.
        ftk::Color4F color     = ftk::Color4F(1.F, 1.F, 1.F, .5F);

        bool operator == (const SafeAreas&) const = default;
    };

    //! Data window: the outline of the pixels a file holds, which can be
    //! smaller than the picture, or reach outside it, as an OpenEXR data
    //! window can. Drawn for images whose tags give one (see
    //! tl::getDataWindow()). The lines are view pixels wide.
    struct TL_TIMELINE_API_TYPE DataWindow
    {
        //! Whether the data window is drawn.
        bool         enabled   = false;
        //! The line width, in view pixels, inside the data window's edge.
        int          lineWidth = 2;
        //! The line color.
        ftk::Color4F color     = ftk::Color4F(1.F, .75F, 0.F);

        bool operator == (const DataWindow&) const = default;
    };

    //! Foreground options.
    struct TL_TIMELINE_API_TYPE ForegroundOptions
    {
        //! The grid.
        Grid             grid;
        //! The center marker.
        CenterMarker     centerMarker;
        //! The missing frame indicator.
        MissingIndicator missingIndicator;
        //! The clipping warning.
        ClippingWarning  clippingWarning;
        //! The frame guide.
        FrameGuide       frameGuide;
        //! The safe areas.
        SafeAreas        safeAreas;
        //! The data window outline.
        DataWindow       dataWindow;

        bool operator == (const ForegroundOptions&) const = default;
    };

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const Grid&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const CenterMarker&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const MissingIndicator&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const ClippingWarning&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const FrameGuide&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const SafeAreas&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const DataWindow&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const ForegroundOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, Grid&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, CenterMarker&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, MissingIndicator&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, ClippingWarning&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, FrameGuide&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, SafeAreas&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, DataWindow&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, ForegroundOptions&);

    ///@}
}
