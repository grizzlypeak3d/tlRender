// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <opentimelineio/version.h>

#include <opentime/rationalTime.h>
#include <opentime/timeRange.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>

namespace tl
{
    //! \todo Marking a time as unset by giving it a value is what makes the
    //! two types below necessary, and it costs more than the comparisons they
    //! guard against: the marker is a real time, so it goes on to be
    //! subtracted from and added to, and the result is wrong without ever
    //! looking wrong. std::optional says the same thing in the type, and is
    //! what OTIO itself uses for this -- an image sequence reference holds a
    //! std::optional<TimeRange>. Moving to it changes the signatures of
    //! Timeline, Player, ThumbnailSystem and IOInfo, so it wants doing in one
    //! pass rather than a member at a time: two conventions at once would be
    //! worse than either. When it is done, everything down to isValid() goes.

    //! Invalid time.
    //!
    //! This converts to a time but cannot be compared with one, because
    //! comparing times rescales them and this is worth exactly one second:
    //! any time of one second -- frame 24 at 24fps -- compares equal to it.
    //! Use isValid() instead, which the deleted operators below insist on.
    struct InvalidTime
    {
        constexpr operator OTIO_NS::RationalTime() const
        {
            return OTIO_NS::RationalTime(-1.0, -1.0);
        }
    };
    constexpr InvalidTime invalidTime;

    //! Invalid time range. See InvalidTime.
    struct InvalidTimeRange
    {
        constexpr operator OTIO_NS::TimeRange() const
        {
            return OTIO_NS::TimeRange(invalidTime, invalidTime);
        }
    };
    constexpr InvalidTimeRange invalidTimeRange;

    //! \name Deleted Comparisons
    //!
    //! Chosen over the conversion to a time, so that comparing against these
    //! markers does not compile. Assigning them still does.
    ///@{

    bool operator == (const OTIO_NS::RationalTime&, InvalidTime) = delete;
    bool operator != (const OTIO_NS::RationalTime&, InvalidTime) = delete;
    bool operator == (InvalidTime, const OTIO_NS::RationalTime&) = delete;
    bool operator != (InvalidTime, const OTIO_NS::RationalTime&) = delete;

    bool operator == (const OTIO_NS::TimeRange&, InvalidTimeRange) = delete;
    bool operator != (const OTIO_NS::TimeRange&, InvalidTimeRange) = delete;
    bool operator == (InvalidTimeRange, const OTIO_NS::TimeRange&) = delete;
    bool operator != (InvalidTimeRange, const OTIO_NS::TimeRange&) = delete;

    ///@}

    //! Check whether the given time is valid. This function should be
    //! used instead of comparing a time to the "invalidTime" constant.
    bool isValid(const OTIO_NS::RationalTime&);

    //! Check whether the given time range is valid. This function
    //! should be used instead of comparing a time range to the
    //! "invalidTimeRange" constant.
    bool isValid(const OTIO_NS::TimeRange&);

    //! Compare two time ranges. This function compares the values
    //! exactly, unlike the "==" operator which rescales the values.
    constexpr bool compareExact(const OTIO_NS::TimeRange&, const OTIO_NS::TimeRange&);

    //! Get the frames in a time range.
    TL_API std::vector<OTIO_NS::RationalTime> frames(const OTIO_NS::TimeRange&);

    //! Split a time range at into seconds.
    TL_API std::vector<OTIO_NS::TimeRange> seconds(const OTIO_NS::TimeRange&);

    //! Convert a floating point rate to a rational.
    TL_API std::pair<int, int> toRational(double);

    //! \name Keycode
    ///@{

    TL_API std::string keycodeToString(
        int id,
        int type,
        int prefix,
        int count,
        int offset);

    TL_API void stringToKeycode(
        const std::string&,
        int& id,
        int& type,
        int& prefix,
        int& count,
        int& offset);

    ///@}

    //! \name Timecode
    ///@{

    TL_API void timecodeToTime(
        uint32_t,
        int& hour,
        int& minute,
        int& second,
        int& frame);

    TL_API uint32_t timeToTimecode(
        int hour,
        int minute,
        int second,
        int frame);

    TL_API std::string timecodeToString(uint32_t);

    TL_API void stringToTimecode(const std::string&, uint32_t&);

    ///@}
}

namespace opentime
{
    namespace OPENTIME_VERSION_NS
    {
        TL_API std::string to_string(const RationalTime&);
        TL_API std::string to_string(const TimeRange&);

        TL_API bool from_string(const std::string&, RationalTime&);
        TL_API bool from_string(const std::string&, TimeRange&);

        TL_API void to_json(nlohmann::json&, const RationalTime&);
        TL_API void to_json(nlohmann::json&, const TimeRange&);

        TL_API void from_json(const nlohmann::json&, RationalTime&);
        TL_API void from_json(const nlohmann::json&, TimeRange&);

        TL_API std::ostream& operator << (std::ostream&, const RationalTime&);
        TL_API std::ostream& operator << (std::ostream&, const TimeRange&);

        TL_API bool cmdLineParse(std::vector<std::string>&, std::vector<std::string>::iterator&, RationalTime&);
        TL_API bool cmdLineParse(std::vector<std::string>&, std::vector<std::string>::iterator&, TimeRange&);
    }
}

#include <tlRender/Core/TimeInline.h>
