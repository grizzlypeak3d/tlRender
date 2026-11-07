// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <ftk/Core/Image.h>

#include <memory>

namespace tl
{
    //! Get whether color tags say Display P3: "Color Primaries" "smpte432"
    //! with the sRGB curve, as an iPhone's photos and a Mac's movies are.
    TL_CORE_API bool isDisplayP3(const ftk::ImageTags&);

    //! Convert eight bit Display P3 pixels, RGBA, to sRGB in place, the
    //! colors past sRGB's clipped: for a picture shown where sRGB is taken
    //! for granted, as a thumbnail is. Any other type is left as it is.
    TL_CORE_API void displayP3ToSRGB(const std::shared_ptr<ftk::Image>&);
}
