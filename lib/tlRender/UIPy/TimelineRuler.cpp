// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/TimelineRuler.h>

#include <tlRender/UI/TimelineRuler.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timelineRuler(py::module_& m)
        {
            using namespace ui;

            py::class_<TimelineRuler, ftk::IMouseWidget, std::shared_ptr<TimelineRuler> >(m, "TimelineRuler")
                .def(
                    py::init(&TimelineRuler::create),
                    py::arg("context"),
                    py::arg("itemData"),
                    py::arg("parent") = nullptr)
                .def("setItemData", &TimelineRuler::setItemData)
                .def("setPlayer", &TimelineRuler::setPlayer)
                .def("setScale", &TimelineRuler::setScale)
                .def("setScrollPos", &TimelineRuler::setScrollPos)
                .def("setOffset", &TimelineRuler::setOffset)
                .def("setFrameMarkers", &TimelineRuler::setFrameMarkers)
                .def("setDisplayOptions", &TimelineRuler::setDisplayOptions)
                .def("setOptions", &TimelineRuler::setOptions)
                .def("setStopOnScrub", &TimelineRuler::setStopOnScrub)
                .def_property_readonly(
                    "observeScrub",
                    &TimelineRuler::observeScrub)
                .def("timeToPos", &TimelineRuler::timeToPos);
        }
    }
}

