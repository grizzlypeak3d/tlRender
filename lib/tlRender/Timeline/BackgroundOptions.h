// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Color.h>
#include <ftk/Core/Size.h>
#include <ftk/Core/Util.h>

namespace tl
{
    //! Background type.
    enum class TL_TIMELINE_API_TYPE Background
    {
        Solid,
        Checkers,
        Gradient,

        Count,
        First = Solid
    };
    FTK_ENUM(TL_TIMELINE_API, Background);

    //! Outline.
    struct TL_TIMELINE_API_TYPE Outline
    {
        //! Whether an outline is drawn around each image shown.
        bool         enabled = false;
        //! The outline width, in view pixels, outside the image's edge.
        int          width   = 2;
        //! The outline color.
        ftk::Color4F color   = ftk::Color4F(1.F, 0.F, 0.F);

        bool operator == (const Outline&) const = default;
    };

    //! Background options.
    struct TL_TIMELINE_API_TYPE BackgroundOptions
    {
        //! The background type.
        Background type = Background::Solid;

        //! The color for Background::Solid.
        ftk::Color4F solidColor = ftk::Color4F(0.F, 0.F, 0.F);

        //! The two checker colors, for Background::Checkers.
        std::pair<ftk::Color4F, ftk::Color4F> checkersColor =
        {
            ftk::Color4F(0.F, 0.F, 0.F),
            ftk::Color4F(1.F, 1.F, 1.F)
        };
        //! The size of each checker, in view pixels.
        ftk::Size2I checkersSize = ftk::Size2I(100, 100);

        //! The top and bottom colors, for Background::Gradient.
        std::pair<ftk::Color4F, ftk::Color4F> gradientColor =
        {
            ftk::Color4F(0.F, 0.F, 0.F),
            ftk::Color4F(1.F, 1.F, 1.F)
        };

        //! The outline around the images.
        Outline outline;

        bool operator == (const BackgroundOptions&) const = default;
    };

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const Outline&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const BackgroundOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, Outline&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, BackgroundOptions&);

    ///@}
}
