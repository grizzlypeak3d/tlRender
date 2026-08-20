// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <ftk/Core/Context.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void coreBind(py::module_& m)
        {
            audio(m);
            audioResample(m);
            hdr(m);
            time(m);
            url(m);
        }
    }
}

