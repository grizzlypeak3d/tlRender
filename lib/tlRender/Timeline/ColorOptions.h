// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Image.h>
#include <ftk/Core/Util.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace tl
{
    //! OpenColorIO configuration options.
    enum class TL_TIMELINE_API_TYPE OCIOConfig
    {
        BuiltIn,
        EnvVar,
        File,

        Count,
        First = BuiltIn
    };
    FTK_ENUM(TL_TIMELINE_API, OCIOConfig);

    //! OpenColorIO options.
    struct TL_TIMELINE_API_TYPE OCIOOptions
    {
        bool        enabled  = false;
        OCIOConfig  config   = OCIOConfig::BuiltIn;
        std::string fileName;
        std::string input;
        std::string display;
        std::string view;
        std::string look;

        TL_TIMELINE_API bool operator == (const OCIOOptions&) const;
        TL_TIMELINE_API bool operator != (const OCIOOptions&) const;
    };

    //! Get the color description tags for pixels rendered through the
    //! display transform of the given options: the display's encoding.
    //! The common displays of the OpenColorIO configurations are named;
    //! an unrecognized display, or options that do not render through a
    //! display transform, return nothing rather than guess. Sequences
    //! take the "Chromaticities" tag, movies "Color Primaries" and
    //! "Color Transfer"; the YUV matrix is left unsaid either way, since
    //! it belongs to the encoder's conversion, not the display.
    TL_TIMELINE_API ftk::ImageTags getDisplayColorTags(
        const OCIOOptions&,
        bool sequence);

    //! LUT operation order.
    enum class TL_TIMELINE_API_TYPE LUTOrder
    {
        PostConfig,
        PreConfig,

        Count,
        First = PostConfig
    };
    FTK_ENUM(TL_TIMELINE_API, LUTOrder);

    //! LUT direction.
    //!
    //! Forward is the direction the file itself reads. An ICC monitor
    //! profile reads from the monitor's code values, so correcting the
    //! display with one takes Inverse.
    enum class TL_TIMELINE_API_TYPE LUTDirection
    {
        Forward,
        Inverse,

        Count,
        First = Forward
    };
    FTK_ENUM(TL_TIMELINE_API, LUTDirection);

    //! LUT options.
    struct TL_TIMELINE_API_TYPE LUTOptions
    {
        bool         enabled   = false;
        std::string  fileName;
        LUTDirection direction = LUTDirection::First;
        LUTOrder     order     = LUTOrder::First;

        TL_TIMELINE_API bool operator == (const LUTOptions&) const;
        TL_TIMELINE_API bool operator != (const LUTOptions&) const;
    };

    //! Get the list of LUT format names.
    TL_TIMELINE_API std::vector<std::string> getLUTFormatNames();

    //! Get the list of LUT format file extensions.
    TL_TIMELINE_API std::vector<std::string> getLUTFormatExts();

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const OCIOOptions&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const LUTOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, OCIOOptions&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, LUTOptions&);

    ///@}
}
