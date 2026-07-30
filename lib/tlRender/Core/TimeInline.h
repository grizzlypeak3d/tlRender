// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

namespace tl
{
    constexpr bool compareExact(const OTIO_NS::TimeRange& a, const OTIO_NS::TimeRange& b)
    {
        return
            a.start_time().strictly_equal(b.start_time()) &&
            a.duration().strictly_equal(b.duration());
    }

    constexpr bool compareExact(
        const std::optional<OTIO_NS::RationalTime>& a,
        const std::optional<OTIO_NS::RationalTime>& b)
    {
        return a.has_value() && b.has_value() ?
            a.value().strictly_equal(b.value()) :
            a.has_value() == b.has_value();
    }

    constexpr bool compareExact(
        const std::optional<OTIO_NS::TimeRange>& a,
        const std::optional<OTIO_NS::TimeRange>& b)
    {
        return a.has_value() && b.has_value() ?
            compareExact(a.value(), b.value()) :
            a.has_value() == b.has_value();
    }
}

