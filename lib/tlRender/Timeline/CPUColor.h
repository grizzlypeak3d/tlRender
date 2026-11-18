// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/ColorShaders.h>

#include <cstdint>
#include <memory>
#include <vector>

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

    //! A CPUColor sampled into a table, for 8-bit pixels: a look up and an
    //! interpolation for each, where the color itself is hundreds of
    //! nanoseconds a pixel through an ACES 2.0 view -- tens of
    //! milliseconds for one thumbnail. Close to the color rather than
    //! exact, which is what a thumbnail needs. Alpha is passed through, so
    //! it is not for the alpha channel display.
    class TL_TIMELINE_API_TYPE CPUColorTable
    {
    public:
        //! Sample the color, with this many points along each axis.
        TL_TIMELINE_API explicit CPUColorTable(const CPUColor&, int size = 33);

        TL_TIMELINE_API ~CPUColorTable();

        //! Apply the color to RGBA pixels, four bytes each, in place.
        TL_TIMELINE_API void apply(uint8_t* rgba, size_t count) const;

    private:
        int _size = 0;
        std::vector<float> _table;
    };
#endif // TLRENDER_OCIO

    ///@}
}
