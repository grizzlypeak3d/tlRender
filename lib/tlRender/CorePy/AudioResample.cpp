// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <tlRender/Core/AudioResample.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void audioResample(nb::module_& m)
        {
            nb::class_<AudioResample>(m, "AudioResample")
                .def(nb::new_(&AudioResample::create),
                    nb::arg("input"),
                    nb::arg("output"))
                .def_prop_ro("inputInfo", &AudioResample::getInputInfo, nb::rv_policy::copy)
                .def_prop_ro("outputInfo", &AudioResample::getOutputInfo, nb::rv_policy::copy)
                .def("process", &AudioResample::process, nb::arg("audio"))
                .def("flush", &AudioResample::flush);
        }
    }
}
