// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <ftk/Core/Image.h>

namespace tl
{
    //! Scale image data on the CPU, converting between image types as
    //! needed. Like AudioResample this is built on FFmpeg; without it
    //! process() returns nothing.
    class TL_CORE_API_TYPE ImageScale
    {
        FTK_NON_COPYABLE(ImageScale);

    protected:
        void _init(
            const ftk::ImageInfo& input,
            const ftk::ImageInfo& output);

        ImageScale();

    public:
        TL_CORE_API ~ImageScale();

        //! Create a new scaler.
        TL_CORE_API static std::shared_ptr<ImageScale> create(
            const ftk::ImageInfo& input,
            const ftk::ImageInfo& output);

        //! Get the input image information.
        TL_CORE_API const ftk::ImageInfo& getInputInfo() const;

        //! Get the output image information.
        TL_CORE_API const ftk::ImageInfo& getOutputInfo() const;

        //! Scale image data.
        TL_CORE_API std::shared_ptr<ftk::Image> process(
            const std::shared_ptr<ftk::Image>&);

    private:
        FTK_PRIVATE();
    };
}
