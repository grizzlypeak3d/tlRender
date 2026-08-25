// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/TimeEdit.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timeEdit(py::module_& m)
        {
            using namespace ui;

            py::class_<TimeMap>(m, "TimeMap")
                .def(py::init())
                .def_readwrite("toMedia", &TimeMap::toMedia)
                .def_readwrite("fromMedia", &TimeMap::fromMedia);

            py::class_<TimeEdit, ftk::IWidget, std::shared_ptr<TimeEdit> >(m, "TimeEdit")
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<TimeUnitsModel>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimeEdit::create)),
                    py::arg("context"),
                    py::arg("timeUnitsModel"),
                    py::arg("parent") = nullptr)
                .def_property("value", &TimeEdit::getValue, &TimeEdit::setValue)
                .def("setCallback", &TimeEdit::setCallback)
                .def("setTimeMap", &TimeEdit::setTimeMap)
                .def("selectAll", &TimeEdit::selectAll);
        }
    }
}

