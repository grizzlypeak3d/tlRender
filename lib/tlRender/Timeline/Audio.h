// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/IO/IO.h>

namespace tl
{
    //! Audio layer.
    struct TL_TIMELINE_API_TYPE AudioLayer
    {
        //! Audio data: one second of it, silent where the clip does not cover
        //! the second.
        std::shared_ptr<Audio> audio;

        bool operator == (const AudioLayer&) const = default;
    };

    //! Audio frame.
    struct TL_TIMELINE_API_TYPE AudioFrame
    {
        //! Time the frame starts at, in seconds. A frame is one second of
        //! audio.
        double                  seconds = -1.0;
        //! One layer for each audio clip under the frame's second, or one
        //! silent layer when there are none.
        std::vector<AudioLayer> layers;

        bool operator == (const AudioFrame&) const = default;
    };

    //! Compare the time values of audio frames.
    TL_TIMELINE_API bool isTimeEqual(const AudioFrame&, const AudioFrame&);
}
