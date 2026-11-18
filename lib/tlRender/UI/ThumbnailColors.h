// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>

#include <tlRender/Timeline/ColorOptions.h>

#include <ftk/Core/Image.h>

#include <functional>

namespace tl
{
    namespace ui
    {
        //! Thumbnails in the color the picture is drawn with.
        //!
        //! A thumbnail is read as it is in the file and drawn as an image,
        //! which the color options do not touch. This puts each one through
        //! them on the CPU, with the input color space of the media it came
        //! from (see thumbnailPathTag), and keeps it until the color
        //! changes. Through a table sampled from the color (CPUColorTable),
        //! as the color itself is too slow for the user interface's thread.
        class TL_UI_API_TYPE ThumbnailColors
        {
        public:
            TL_UI_API ThumbnailColors();

            TL_UI_API ~ThumbnailColors();

            //! Set the OpenColorIO options. An empty input is resolved for
            //! each thumbnail.
            TL_UI_API void setOCIOOptions(const OCIOOptions&);

            //! Set the LUT options.
            TL_UI_API void setLUTOptions(const LUTOptions&);

            //! Set a resolver for each thumbnail's input color space, as
            //! IRender::setOCIOInputResolver(): given the path of the media
            //! the thumbnail came from and its tags, it names the input, or
            //! nothing to use the options' own.
            TL_UI_API void setOCIOInputResolver(
                const std::function<std::string(
                    const std::string& path,
                    const ftk::ImageTags&)>&);

            //! Get a thumbnail in color: the same one when there is nothing
            //! to do. A thumbnail that is not 8-bit RGBA is not colored.
            TL_UI_API std::shared_ptr<ftk::Image> get(
                const std::shared_ptr<ftk::Image>&);

            //! Let go of the thumbnails in color whose thumbnail is held by
            //! nothing else.
            TL_UI_API void prune();

        private:
            void _clear();

            FTK_PRIVATE();
        };
    }
}
