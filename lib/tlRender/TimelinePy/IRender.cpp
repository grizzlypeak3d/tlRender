// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/IRender.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void iRender(py::module_& m)
        {
            py::class_<IRender, ftk::IRender, std::shared_ptr<IRender> >(m, "IRender")
                .def("setOCIOOptions", &IRender::setOCIOOptions)
                .def("setOCIOInputResolver", &IRender::setOCIOInputResolver)
                .def("drawBackground", &IRender::drawBackground)
                .def("drawForeground", &IRender::drawForeground);
        }
    }
}
