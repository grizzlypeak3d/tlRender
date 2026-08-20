// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/TimelineWidget.h>

#include <tlRender/UI/TimelineWidget.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timelineWidget(py::module_& m)
        {
            using namespace ui;

            py::class_<TimelineWidget, ftk::IWidget, std::shared_ptr<TimelineWidget> >(m, "TimelineWidget")
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimelineWidget::create)),
                    py::arg("context"),
                    py::arg("parent") = nullptr)
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<ITimeUnitsModel>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimelineWidget::create)),
                    py::arg("context"),
                    py::arg("timeUnitsModel"),
                    py::arg("parent") = nullptr)
                .def_property_readonly(
                    "timeUnitsModel",
                    &TimelineWidget::getTimeUnitsModel)
                .def_property(
                    "player",
                    &TimelineWidget::getPlayer,
                    &TimelineWidget::setPlayer)
                .def_property(
                    "displayOptions",
                    &TimelineWidget::getDisplayOptions,
                    &TimelineWidget::setDisplayOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeDisplayOptions",
                    &TimelineWidget::observeDisplayOptions);
        }
    }
}

