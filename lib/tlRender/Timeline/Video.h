// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/Transition.h>

#include <tlRender/Core/Time.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/RenderOptions.h>

#include <optional>

namespace tl
{
    //! Video layer.
    struct TL_TIMELINE_API_TYPE VideoLayer
    {
        //! The image, or null when there is none. During a transition, the
        //! image of the clip going out.
        std::shared_ptr<ftk::Image> image;
        //! The options "image" is drawn with when the renderer is not given
        //! options of its own.
        ftk::ImageOptions           imageOptions;

        //! During a transition, the image of the clip coming in; null
        //! otherwise.
        std::shared_ptr<ftk::Image> imageB;
        //! The options "imageB" is drawn with when the renderer is not given
        //! options of its own.
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

        //! The transition the layer is in at this time, or None.
        Transition                  transition      = Transition::None;
        //! How far through the transition the frame is, from zero at its
        //! start to one at its end.
        float                       transitionValue = 0.F;

        //! The path of the media the image came from, so a consumer can
        //! resolve things about the source; e.g., an input color space.
        std::string                 path;

        //! The path of the media "imageB" came from.
        std::string                 pathB;

        //! Per layer overrides of the OCIO input color space; empty uses
        //! the item's. Filled by a consumer that resolves them from the
        //! paths and the image tags; the timeline itself says nothing
        //! about color.
        std::string                 ocioInput;
        std::string                 ocioInputB;

        //! Whether the image stands in for a frame the media does not have,
        //! rather than being the frame that was asked for. Carried per layer
        //! so that a comparison can say which of its sources it is about.
        // Equality includes this: a layer that has become a stand-in, or
        // stopped being one, is a different thing to draw even when the
        // image is the same.
        bool                        missing         = false;

        //! The frame repeated in place of it, when there was one to repeat.
        std::optional<int64_t>      heldFrom;

        //! Whether a reader declined the frame, the playhead having passed
        //! it while playing; see IVideoRead::setPlayhead(). The player
        //! drops such a frame rather than caching it.
        bool                        declined        = false;

        bool operator == (const VideoLayer&) const = default;
    };

    //! Video frame.
    struct TL_TIMELINE_API_TYPE VideoFrame
    {
        //! The size of the timeline's video, from its I/O information
        //! (Timeline::getIOInfo()). The layer images may be other sizes.
        ftk::Size2I             size;

        //! The size of the canvas shared by the whole timeline, when any clip
        //! provides OTIO spatial coordinates. The layer boxes are positioned
        //! within it. Empty otherwise, which lays the frame out from the
        //! image sizes instead.
        ftk::Size2I             canvasSize;

        //! The time on the timeline the frame is for.
        OTIO_NS::RationalTime   time;
        //! The layers, one for each enabled video track with an item at the
        //! time, in track order.
        std::vector<VideoLayer> layers;

        TL_TIMELINE_API bool operator == (const VideoFrame&) const;
        TL_TIMELINE_API bool operator != (const VideoFrame&) const;
    };

    //! Compare the time values of video frames.
    TL_TIMELINE_API bool isTimeEqual(const VideoFrame&, const VideoFrame&);
}
