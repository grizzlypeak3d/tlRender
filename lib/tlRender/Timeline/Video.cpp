// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Video.h>

namespace tl
{
    bool VideoLayer::operator == (const VideoLayer& other) const
    {
        return
            image == other.image &&
            imageOptions == other.imageOptions &&
            imageB == other.imageB &&
            imageOptionsB == other.imageOptionsB &&
            bounds == other.bounds &&
            boundsB == other.boundsB &&
            transition == other.transition &&
            transitionValue == other.transitionValue &&
            // A layer that has become a stand-in, or stopped being one, is a
            // different thing to draw even when the image is the same.
            missing == other.missing &&
            heldFrom == other.heldFrom;
    }

    bool VideoLayer::operator != (const VideoLayer& other) const
    {
        return !(*this == other);
    }

    bool VideoFrame::operator == (const VideoFrame& other) const
    {
        return
            size == other.size &&
            canvasSize == other.canvasSize &&
            time.strictly_equal(other.time) &&
            layers == other.layers;
    }

    bool VideoFrame::operator != (const VideoFrame& other) const
    {
        return !(*this == other);
    }

    bool isTimeEqual(const VideoFrame& a, const VideoFrame& b)
    {
        return a.time.strictly_equal(b.time);
    }
}
