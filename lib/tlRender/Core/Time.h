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
#include <optional>

namespace tl
{
    //! Compare two time ranges. This function compares the values
    //! exactly, unlike the "==" operator which rescales the values.
    constexpr bool compareExact(const OTIO_NS::TimeRange&, const OTIO_NS::TimeRange&);

    //! \name Optional Comparisons
    //!
    //! Either side may be unset. std::optional forwards to the "==" operator,
    //! which rescales, so these are what an optional time is compared with.
    //! The plain time case is RationalTime::strictly_equal().
    ///@{

    constexpr bool compareExact(
        const std::optional<OTIO_NS::RationalTime>&,
        const std::optional<OTIO_NS::RationalTime>&);

    constexpr bool compareExact(
        const std::optional<OTIO_NS::TimeRange>&,
        const std::optional<OTIO_NS::TimeRange>&);

    ///@}

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
