// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Audio.h>

namespace tl
{

    bool isTimeEqual(const AudioFrame& a, const AudioFrame& b)
    {
        return a.seconds == b.seconds;
    }
}
