// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <ftk/Core/Image.h>

#include <cstdint>
#include <vector>

namespace tl
{
    //! Get an ICC profile for display encoded pixels as color tags say
    //! they are: the primaries and white from "Chromaticities" (eight
    //! numbers, red, green, blue and white x and y) or "Color Primaries"
    //! ("bt709", "smpte432" for Display P3, "bt2020"), and the curve from
    //! "Color Transfer" ("iec61966-2-1" for the sRGB curve, "bt709" for
    //! Rec. 1886's 2.4 gamma, "gamma22"). A version 4 display profile, a
    //! matrix and three curves, as an image file carries one, so that
    //! what reads the file shows the colors meant rather than taking them
    //! for sRGB. Empty when the tags say nothing a profile can hold: an
    //! HDR curve, linear light, or no colors at all.
    TL_CORE_API std::vector<uint8_t> getICCProfile(const ftk::ImageTags&);
}
