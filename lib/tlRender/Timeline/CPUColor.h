// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/ColorShaders.h>

#include <memory>

namespace tl
{
    //! \name Renderers
    ///@{

#if defined(TLRENDER_OCIO)
    //! The color a picture is drawn through, applied on the CPU to pixels in
    //! memory: the color stages in getColorStages()'s order, through
    //! OpenColorIO's CPU processors, and the corrections and the rest of the
    //! display shader's body with the same arithmetic as ColorShaders. What
    //! is not color -- sampling, mirroring, enlarging -- is not here.
    //!
    //! The thumbnails are drawn through it, and the renderers are tested
    //! against it, so that the three cannot drift apart unnoticed.
    class TL_TIMELINE_API_TYPE CPUColor
    {
    public:
        //! Make the color for a set of options. The OCIO options' input is
        //! the one used: resolve a layer's first. A Display P3 display is
        //! taken into extended sRGB, as a window holds it, when for a window.
        //! Throws when a processor cannot be had.
        TL_TIMELINE_API CPUColor(
            const OCIOOptions&,
            const LUTOptions&,
            const DisplayOptions& = DisplayOptions(),
            bool window = false);

        TL_TIMELINE_API ~CPUColor();

        //! Get whether the OCIO display is Display P3.
        TL_TIMELINE_API bool isDisplayP3() const;

        //! Apply the color to RGBA pixels, four floats each, in place.
        TL_TIMELINE_API void apply(float* rgba, size_t count) const;

    private:
        FTK_PRIVATE();
    };
#endif // TLRENDER_OCIO

    ///@}
}
