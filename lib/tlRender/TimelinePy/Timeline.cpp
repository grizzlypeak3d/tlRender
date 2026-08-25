// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timeline(py::module_& m)
        {
            py::class_<Timeline, std::shared_ptr<Timeline> >(m, "Timeline")
                .def(py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const ftk::Path&,
                        const Options&>(&Timeline::create)),
                    py::arg("context"),
                    py::arg("path"),
                    py::arg("options") = Options())
                .def(py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const ftk::Path&,
                        const ftk::Path&,
                        const Options&>(&Timeline::create)),
                    py::arg("context"),
                    py::arg("path"),
                    py::arg("audioPath"),
                    py::arg("options") = Options())
                .def(py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::string&,
                        const Options&>(&Timeline::create)),
                    py::arg("context"),
                    py::arg("fileName"),
                    py::arg("options") = Options())
                .def(py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::string&,
                        const std::string&,
                        const Options&>(&Timeline::create)),
                    py::arg("context"),
                    py::arg("fileName"),
                    py::arg("audioFileName"),
                    py::arg("options") = Options())
                .def_property_readonly("context", &Timeline::getContext)
                .def_property_readonly("otioTimeline", &Timeline::getOTIOTimeline)
                .def_property_readonly("path", &Timeline::getPath, py::return_value_policy::copy)
                .def_property_readonly("audioPath", &Timeline::getAudioPath, py::return_value_policy::copy)
                .def_property_readonly("options", &Timeline::getOptions, py::return_value_policy::copy)
                .def_property_readonly("timeRange", &Timeline::getTimeRange, py::return_value_policy::copy)
                .def_property_readonly("duration", &Timeline::getDuration)
                .def_property_readonly("ioInfo", &Timeline::getIOInfo, py::return_value_policy::copy)
                .def("getMediaTime", &Timeline::getMediaTime, py::arg("time"))
                .def(
                    "getTimelineTime",
                    &Timeline::getTimelineTime,
                    py::arg("time"),
                    py::arg("mediaTime"))
                .def("getMediaFrame", &Timeline::getMediaFrame, py::arg("time"))
                .def(
                    "getMediaFrameTime",
                    &Timeline::getMediaFrameTime,
                    py::arg("time"),
                    py::arg("frame"))
                .def("isMediaTimeContinuous", &Timeline::isMediaTimeContinuous);
        }
    }
}
