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
    TL_ENUM_IMPL(
        ImageSeqAudio,
        "None",
        "Ext",
        "FileName");

    TL_ENUM_IMPL(
        Spatial,
        "None",
        "Coordinates",
        "Normalize");

    size_t getDefaultReadThreadCount()
    {
        return std::max(1u, std::thread::hardware_concurrency());
    }

    bool Options::operator == (const Options& other) const
    {
        return
            imageSeqAudio == other.imageSeqAudio &&
            spatial == other.spatial &&
            imageSeqAudioExts == other.imageSeqAudioExts &&
            imageSeqAudioFileName == other.imageSeqAudioFileName &&
            compat == other.compat &&
            threaded == other.threaded &&
            readThreadCount == other.readThreadCount &&
            videoRequestMax == other.videoRequestMax &&
            audioRequestMax == other.audioRequestMax &&
            requestTimeout == other.requestTimeout &&
            ioOptions == other.ioOptions &&
            pathOptions == other.pathOptions;
    }

    bool Options::operator != (const Options& other) const
    {
        return !(*this == other);
    }
}
