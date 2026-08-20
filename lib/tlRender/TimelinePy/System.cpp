// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/System.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timelineSystem(py::module_& m)
        {
            py::class_<System, ftk::ISystem, std::shared_ptr<System> >(m, "System")
                .def(py::init(&System::create),
                    py::arg("context"));
        }
    }
}
