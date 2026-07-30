// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Transition.h>

#include <tlRender/Core/Time.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/RenderOptions.h>

#include <optional>

namespace tl
{
    //! Video layer.
    struct TL_API_TYPE VideoLayer
    {
        std::shared_ptr<ftk::Image> image;
        ftk::ImageOptions           imageOptions;

        std::shared_ptr<ftk::Image> imageB;
        ftk::ImageOptions           imageOptionsB;

        //! The box the image occupies within the timeline canvas, when the
        //! clip provides OTIO spatial coordinates (see the
        //! "available_image_bounds" media reference property). Clips that
        //! share the same box are displayed at the same size and position
        //! regardless of their image resolution.
        //!
        //! This has been converted from the OTIO coordinate system: scaled
        //! from unit-less coordinates to pixels, flipped from Y-up to Y-down,
        //! and translated so the canvas starts at the origin.
        std::optional<ftk::Box2F>   bounds;

        //! The canvas box for "imageB", which comes from the neighbouring
        //! clip during a transition and may be placed differently.
        std::optional<ftk::Box2F>   boundsB;

        Transition                  transition      = Transition::None;
        float                       transitionValue = 0.F;

        TL_API bool operator == (const VideoLayer&) const;
        TL_API bool operator != (const VideoLayer&) const;
    };

    //! Video frame.
    struct TL_API_TYPE VideoFrame
    {
        ftk::Size2I             size;

        //! The size of the canvas shared by the whole timeline, when any clip
        //! provides OTIO spatial coordinates. The layer boxes are positioned
        //! within it. Empty otherwise, which lays the frame out from the
        //! image sizes instead.
        ftk::Size2I             canvasSize;

        OTIO_NS::RationalTime   time;
        std::vector<VideoLayer> layers;

        TL_API bool operator == (const VideoFrame&) const;
        TL_API bool operator != (const VideoFrame&) const;
    };

    //! Compare the time values of video frames.
    TL_API bool isTimeEqual(const VideoFrame&, const VideoFrame&);
}
