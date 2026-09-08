// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <ftk/Core/Context.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void ioBind(nb::module_& m)
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

