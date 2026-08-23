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

        TL_TIMELINE_API bool operator == (const AudioLayer&) const;
        TL_TIMELINE_API bool operator != (const AudioLayer&) const;
    };

    //! Audio frame.
    struct TL_TIMELINE_API_TYPE AudioFrame
    {
        double                  seconds = -1.0;
        std::vector<AudioLayer> layers;

        TL_TIMELINE_API bool operator == (const AudioFrame&) const;
        TL_TIMELINE_API bool operator != (const AudioFrame&) const;
    };

    //! Compare the time values of audio frames.
    TL_TIMELINE_API bool isTimeEqual(const AudioFrame&, const AudioFrame&);
}
