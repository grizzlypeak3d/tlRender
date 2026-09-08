// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <opentimelineio/version.h>
#include <opentime/rationalTime.h>
#include <opentime/timeRange.h>

#include <nanobind/nanobind.h>

//! Casters between tlRender's C++ OTIO types and the opentimelineio
//! Python package, converting through the package's own Python API.
//!
//! tlRender links its own OTIO build and the opentimelineio wheel
//! bundles another; the two cannot safely share C++ objects, and the
//! pybind11 bindings' cross-extension sharing only worked while both
//! sides were built with ABI-compatible pybind11 versions. Value
//! conversion through the Python API works with any opentimelineio
//! wheel. Include this header in every binding source that passes
//! opentime types.

namespace nanobind
{
    namespace detail
    {
        template<>
        struct type_caster<OTIO_NS::RationalTime>
        {
            NB_TYPE_CASTER(
                OTIO_NS::RationalTime,
                const_name("opentimelineio.opentime.RationalTime"))

            bool from_python(handle src, uint8_t, cleanup_list*) noexcept
            {
                try
                {
                    value = OTIO_NS::RationalTime(
                        cast<double>(src.attr("value")),
                        cast<double>(src.attr("rate")));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            static handle from_cpp(
                const OTIO_NS::RationalTime& value,
                rv_policy,
                cleanup_list*) noexcept
            {
                try
                {
                    return module_::import_("opentimelineio.opentime")
                        .attr("RationalTime")(value.value(), value.rate())
                        .release();
                }
                catch (...)
                {
                    return handle();
                }
            }
        };

        template<>
        struct type_caster<OTIO_NS::TimeRange>
        {
            NB_TYPE_CASTER(
                OTIO_NS::TimeRange,
                const_name("opentimelineio.opentime.TimeRange"))

            bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept
            {
                try
                {
                    make_caster<OTIO_NS::RationalTime> start;
                    make_caster<OTIO_NS::RationalTime> duration;
                    if (!start.from_python(
                            src.attr("start_time"), flags, cleanup) ||
                        !duration.from_python(
                            src.attr("duration"), flags, cleanup))
                    {
                        return false;
                    }
                    value = OTIO_NS::TimeRange(start.value, duration.value);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            static handle from_cpp(
                const OTIO_NS::TimeRange& value,
                rv_policy policy,
                cleanup_list* cleanup) noexcept
            {
                try
                {
                    object start = steal(
                        make_caster<OTIO_NS::RationalTime>::from_cpp(
                            value.start_time(), policy, cleanup));
                    object duration = steal(
                        make_caster<OTIO_NS::RationalTime>::from_cpp(
                            value.duration(), policy, cleanup));
                    if (!start.is_valid() || !duration.is_valid())
                        return handle();
                    return module_::import_("opentimelineio.opentime")
                        .attr("TimeRange")(start, duration)
                        .release();
                }
                catch (...)
                {
                    return handle();
                }
            }
        };
    }
}
