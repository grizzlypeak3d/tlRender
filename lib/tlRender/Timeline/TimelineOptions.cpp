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
        return std::max(1u, std::thread::hardware_concurrency());
    }

}
