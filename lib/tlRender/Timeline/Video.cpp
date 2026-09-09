// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Video.h>

namespace tl
{
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
