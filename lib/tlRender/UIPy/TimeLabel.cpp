// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/TimeLabel.h>

#include <tlRender/UI/TimeLabel.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>
#include <pybind11/operators.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timeLabel(py::module_& m)
        {
            using namespace ui;

            py::class_<TimeLabel, ftk::IWidget, std::shared_ptr<TimeLabel> >(m, "TimeLabel")
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<TimeUnitsModel>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimeLabel::create)),
                    py::arg("context"),
                    py::arg("timeUnitsModel"),
                    py::arg("parent") = nullptr)
                .def_property("value", &TimeLabel::getValue, &TimeLabel::setValue)
                .def("setMarginRole", &TimeLabel::setMarginRole);
        }
    }
}

