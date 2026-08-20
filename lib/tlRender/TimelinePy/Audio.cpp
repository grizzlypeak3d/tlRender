// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Audio.h>

#include <ftk/Core/Context.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timelineAudio(py::module_& m)
        {
            py::class_<AudioLayer>(m, "AudioLayer")
                .def(py::init())
                .def_readwrite("audio", &AudioLayer::audio)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<AudioFrame>(m, "AudioFrame")
                .def(py::init())
                .def_readwrite("seconds", &AudioFrame::seconds)
                .def_readwrite("layers", &AudioFrame::layers)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            m.def(
                "isTimeEqual",
                [](const AudioFrame& a, const AudioFrame& b)
                {
                    return isTimeEqual(a, b);
                });
        }
    }
}
