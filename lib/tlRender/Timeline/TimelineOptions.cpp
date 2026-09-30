// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/TimelineOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <algorithm>
#include <sstream>
#include <thread>

namespace tl
{
    FTK_ENUM_IMPL(
        ImageSeqAudio,
        "None",
        "Ext",
        "FileName");

    FTK_ENUM_IMPL(
        Spatial,
        "None",
        "Coordinates",
        "Normalize");

    size_t getDefaultReadThreadCount()
    {
        // Not one for every core. Reading is mostly waiting on the disk, and
        // every read in flight shares it: reading 4K EXRs, thirty-two threads
        // read no more frames a second than eight, and each frame took several
        // times as long, up to seconds for the last of them, so the frame the
        // playhead needed waited behind the others and was dropped. A format
        // that is expensive to decode can use more, and the setting is there
        // for it.
        return std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
    }

}
