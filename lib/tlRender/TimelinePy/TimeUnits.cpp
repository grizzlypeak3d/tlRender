// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timeUnits(py::module_& m)
        {
            py::enum_<TimeUnits>(m, "TimeUnits")
                .value("Frames", TimeUnits::Frames)
                .value("Seconds", TimeUnits::Seconds)
                .value("Timecode", TimeUnits::Timecode);
            FTK_ENUM_BIND(m, TimeUnits);
            ftk::python::observable<TimeUnits>(m, "TimeUnits");

            m.def(
                "timeToText",
                &timeToText,
                py::arg("time"),
                py::arg("units"));

            py::class_<ITimeUnitsModel, std::shared_ptr<ITimeUnitsModel> >(m, "ITimeUnitsModel")
                .def("getLabel", &ITimeUnitsModel::getLabel, py::arg("time"));

            py::class_<TimeUnitsModel, ITimeUnitsModel, std::shared_ptr<TimeUnitsModel> >(m, "TimeUnitsModel")
                .def(py::init(&TimeUnitsModel::create), py::arg("context"))
                .def_property("timeUnits", &TimeUnitsModel::getTimeUnits, &TimeUnitsModel::setTimeUnits)
                .def_property_readonly("observeTimeUnits", &TimeUnitsModel::observeTimeUnits);
        }
    }
}
