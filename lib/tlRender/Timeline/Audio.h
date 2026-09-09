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
        std::shared_ptr<Audio> audio;

        bool operator == (const AudioLayer&) const = default;
    };

    //! Audio frame.
    struct TL_TIMELINE_API_TYPE AudioFrame
    {
        double                  seconds = -1.0;
        std::vector<AudioLayer> layers;

        bool operator == (const AudioFrame&) const = default;
    };

    //! Compare the time values of audio frames.
    TL_TIMELINE_API bool isTimeEqual(const AudioFrame&, const AudioFrame&);
}
