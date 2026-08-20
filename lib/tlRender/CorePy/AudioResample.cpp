// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/AudioResample.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void audioResample(py::module_& m)
        {
            py::class_<AudioResample, std::shared_ptr<AudioResample> >(m, "AudioResample")
                .def(py::init(&AudioResample::create),
                    py::arg("input"),
                    py::arg("output"))
                .def_property_readonly("inputInfo", &AudioResample::getInputInfo, py::return_value_policy::copy)
                .def_property_readonly("outputInfo", &AudioResample::getOutputInfo, py::return_value_policy::copy)
                .def("process", &AudioResample::process, py::arg("audio"))
                .def("flush", &AudioResample::flush);
        }
    }
}
