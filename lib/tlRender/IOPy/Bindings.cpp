// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <ftk/Core/Context.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void ioBind(py::module_& m)
        {
            io(m);
            plugin(m);
            seqIO(m);
            read(m);
            write(m);
            ioSystem(m);
        }
    }
}

