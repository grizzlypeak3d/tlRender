// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/CompareOptions.h>

#include <nlohmann/json.hpp>

#include <ftk/CorePy/Bindings.h>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/optional.h>
#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/array.h>
#include <nanobind/stl/set.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void compareOptions(nb::module_& m)
        {
            FTK_ENUM_PY(m, Compare);
            FTK_ENUM_BIND(m, Compare);

            FTK_ENUM_PY(m, CompareTime);
            FTK_ENUM_BIND(m, CompareTime);

            m.def(
                "getCompareTime",
                &getCompareTime,
                nb::arg("sourceTime"),
                nb::arg("sourceTimeRange"),
                nb::arg("compareTimeRange"),
                nb::arg("compareTime") = CompareTime::Relative,
                "The time in a compared timeline for a time in this one, as "
                "a Player finds it: Relative counts from each timeline's "
                "start, in the compared one's rate, Absolute takes the time "
                "as it is. For Timeline.getVideo() on the compared "
                "timeline.");

            nb::class_<CompareOptions>(
                m, "CompareOptions",
                "How compared timelines are drawn together: compare is the "
                "mode (None for this timeline alone, B, Wipe, Butterfly, "
                "Overlay, Difference, Horizontal, Vertical, Tile), and the "
                "other fields are the settings for the modes "
                "that have them. Read by whatever draws the frames, a "
                "viewport or a renderer; which frames there are is the "
                "Player's compare.")
                .def(nb::init())
                .def_rw("compare", &CompareOptions::compare)
                .def_rw("wipeCenter", &CompareOptions::wipeCenter)
                .def_rw("wipeRotation", &CompareOptions::wipeRotation)
                .def_rw("overlay", &CompareOptions::overlay)
                .def_rw("differenceGain", &CompareOptions::differenceGain)
                .def_rw("sameSize", &CompareOptions::sameSize)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            ftk::python::observable<CompareOptions>(m, "CompareOptions");

            m.def("to_json",
                [](const CompareOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, CompareOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
