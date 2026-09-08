// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

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
#include <nanobind/stl/function.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void timeUnits(nb::module_& m)
        {
            FTK_ENUM_PY(m, TimeUnits);
            FTK_ENUM_BIND(m, TimeUnits);
            ftk::python::observable<TimeUnits>(m, "TimeUnits");

            m.def(
                "timeToText",
                &timeToText,
                nb::arg("time"),
                nb::arg("units"));

            nb::class_<ITimeUnitsModel>(m, "ITimeUnitsModel")
                .def("getLabel", &ITimeUnitsModel::getLabel, nb::arg("time"));

            nb::class_<TimeUnitsModel, ITimeUnitsModel>(m, "TimeUnitsModel")
                .def(nb::new_(&TimeUnitsModel::create), nb::arg("context"))
                .def_prop_rw("timeUnits", &TimeUnitsModel::getTimeUnits, &TimeUnitsModel::setTimeUnits)
                .def_prop_ro("observeTimeUnits", &TimeUnitsModel::observeTimeUnits);
        }
    }
}
