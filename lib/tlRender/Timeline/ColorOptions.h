// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Image.h>
#include <ftk/Core/Util.h>

#include <nlohmann/json_fwd.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace tl
{
    //! OpenColorIO configuration options.
    enum class TL_TIMELINE_API_TYPE OCIOConfig
    {
        //! The configuration built into OpenColorIO, "ocio://default".
        BuiltIn,
        //! The configuration the OCIO environment variable names.
        EnvVar,
        //! The configuration file in OCIOOptions::fileName.
        File,

        Count,
        First = BuiltIn
    };
    FTK_ENUM(TL_TIMELINE_API, OCIOConfig);

    //! OpenColorIO options.
    struct TL_TIMELINE_API_TYPE OCIOOptions
    {
        //! Whether OpenColorIO color management is applied. It takes effect
        //! only with a display, a view, and an input color space (this one's
        //! or DisplayOptions::ocioInput).
        bool        enabled  = false;
        //! Where the configuration comes from.
        OCIOConfig  config   = OCIOConfig::BuiltIn;
        //! The configuration file, for OCIOConfig::File.
        std::string fileName;
        //! The input color space, by name in the configuration.
        std::string input;
        //! The display, by name in the configuration.
        std::string display;
        //! The view, by name in the configuration.
        std::string view;
        //! The look, by name in the configuration; empty for none.
        std::string look;

        bool operator == (const OCIOOptions&) const = default;
    };

    //! Get the color description tags for pixels rendered through the
    //! display transform of the given options: the display's encoding.
    //! The common displays of the OpenColorIO configurations are named;
    //! an unrecognized display, or options that do not render through a
    //! display transform, return nothing rather than guess. Sequences
    //! take the "Chromaticities" tag, movies "Color Primaries"; both take
    //! "Color Transfer", which for a sequence is what its ICC profile is
    //! made from with the chromaticities (see getICCProfile()). The YUV
    //! matrix is left unsaid either way, since it belongs to the
    //! encoder's conversion, not the display.
    TL_TIMELINE_API ftk::ImageTags getDisplayColorTags(
        const OCIOOptions&,
        bool sequence);

    //! LUT operation order.
    enum class TL_TIMELINE_API_TYPE LUTOrder
    {
        //! Applied after the color adjustments and the OpenColorIO display
        //! transform.
        PostConfig,
        //! Applied before the OpenColorIO input conversion and the color
        //! adjustments.
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
        //! Whether the LUT is applied.
        bool         enabled   = false;
        //! The LUT file, in any of the formats getLUTFormatNames() lists.
        std::string  fileName;
        //! The direction the LUT is applied in.
        LUTDirection direction = LUTDirection::First;
        //! Where the LUT is applied among the display's color operations.
        LUTOrder     order     = LUTOrder::First;

        bool operator == (const LUTOptions&) const = default;
    };

    //! Get the list of LUT format names.
    TL_TIMELINE_API std::vector<std::string> getLUTFormatNames();

    //! Get the list of LUT format file extensions.
    TL_TIMELINE_API std::vector<std::string> getLUTFormatExts();

    //! Get whether the OCIO environment variable names a configuration.
    //! An empty variable is not one: OpenColorIO answers a request for it
    //! with a configuration that manages nothing, and says so on stderr
    //! for each one asked for.
    TL_TIMELINE_API bool hasOCIOEnvVar();

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const OCIOOptions&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const LUTOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, OCIOOptions&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, LUTOptions&);

    ///@}
}
