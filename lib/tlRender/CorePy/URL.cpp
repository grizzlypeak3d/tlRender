// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <tlRender/Core/URL.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void url(nb::module_& m)
        {
            m.def("getURLScheme", &getURLScheme, nb::arg("url"));
            m.def("encodeURL", &encodeURL, nb::arg("url"));
            m.def("decodeURL", &decodeURL, nb::arg("url"));
        }
    }
}
