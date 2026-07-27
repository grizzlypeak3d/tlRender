// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/AudioSystem.h>
#include <tlRender/Core/Time.h>

#include <ftk/Core/Memory.h>

namespace tl
{
    //! Timeline player cache options.
    struct TL_API_TYPE PlayerCacheOptions
    {
        //! Video cache size in gigabytes.
        float videoGB = 4.F;

        //! Audio cache size in gigabytes.
        float audioGB = .5F;

        //! Number of seconds to read behind the current frame.
        float readBehind = .5F;

        TL_API bool operator == (const PlayerCacheOptions&) const;
        TL_API bool operator != (const PlayerCacheOptions&) const;
    };

    //! Timeline player options.
    struct TL_API_TYPE PlayerOptions
    {
        //! Audio device index.
        AudioDeviceID audioDevice;

        //! Cache options.
        PlayerCacheOptions cache;

        //! How far ahead the player asks for frames while filling its
        //! cache.
        //!
        //! This is the player expressing demand, not a limit on how much is
        //! decoded at once; the timeline decides that. Keep it at or above
        //! the timeline's readThreadCount, or the decoding threads run out
        //! of work to do.
        size_t videoRequestMax = 16;

        //! How far ahead the player asks for audio while filling its cache.
        //!
        //! The player expressing demand, as with videoRequestMax above; the
        //! timeline decides how much of it is in flight.
        size_t audioRequestMax = 16;

        //! Audio buffer frame count.
        size_t audioBufferFrameCount = 500;

        //! Timeout for muting the audio when playback stutters.
        std::chrono::milliseconds muteTimeout = std::chrono::milliseconds(500);

        //! Timeout to sleep each tick.
        std::chrono::milliseconds sleepTimeout = std::chrono::milliseconds(5);

        //! Current time to start at.
        OTIO_NS::RationalTime currentTime = invalidTime;

        TL_API bool operator == (const PlayerOptions&) const;
        TL_API bool operator != (const PlayerOptions&) const;
    };

    //! \name Serialize
    ///@{

    TL_API void to_json(nlohmann::json&, const PlayerCacheOptions&);

    TL_API void from_json(const nlohmann::json&, PlayerCacheOptions&);

    ///@}
}
