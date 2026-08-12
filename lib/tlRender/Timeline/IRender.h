// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Video.h>

#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Texture.h>
#include <ftk/Core/IRender.h>

namespace tl
{
    //! Base class for renderers.
    class TL_API_TYPE IRender : public ftk::IRender
    {
    public:
        TL_API virtual ~IRender() = 0;

        //! Set the OpenColorIO options.
        TL_API virtual void setOCIOOptions(const OCIOOptions&) = 0;

        //! Set the LUT options.
        TL_API virtual void setLUTOptions(const LUTOptions&) = 0;

        //! Draw the background.
        TL_API virtual void drawBackground(
            const std::vector<ftk::Box2I>&,
            const ftk::M44F& vm,
            const BackgroundOptions&,
            const CompareOptions&) = 0;

        //! Draw timeline video data.
        TL_API virtual void drawVideo(
            const std::vector<VideoFrame>&,
            const std::vector<ftk::Box2I>&,
            const std::vector<ftk::ImageOptions>& = {},
            const std::vector<DisplayOptions>& = {},
            const CompareOptions& = CompareOptions(),
            ftk::gl::TextureType colorBuffer = ftk::gl::offscreenColorDefault) = 0;

        //! Draw the foreground.
        TL_API virtual void drawForeground(
            const std::vector<ftk::Box2I>&,
            const ftk::M44F& vm,
            const ForegroundOptions&,
            const CompareOptions&) = 0;
    };
}
