// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/URL.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void url(py::module_& m)
        {
            m.def("getURLScheme", &getURLScheme, py::arg("url"));
            m.def("encodeURL", &encodeURL, py::arg("url"));
            m.def("decodeURL", &decodeURL, py::arg("url"));
        }
    }
}
