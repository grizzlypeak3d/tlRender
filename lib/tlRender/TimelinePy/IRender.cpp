// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/IRender.h>

#include <ftk/Core/Context.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/optional.h>
#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/array.h>
#include <nanobind/stl/set.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/function.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void iRender(nb::module_& m)
        {
            nb::class_<IRender, ftk::IRender>(m, "IRender")
                .def("setOCIOOptions", &IRender::setOCIOOptions)
                .def("setOCIOInputResolver", &IRender::setOCIOInputResolver)
                .def("drawBackground", &IRender::drawBackground)
                .def("drawForeground", &IRender::drawForeground)
                .def("drawClippingWarning", &IRender::drawClippingWarning);
        }
    }
}
