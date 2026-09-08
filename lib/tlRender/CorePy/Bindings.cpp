// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <ftk/Core/Context.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void coreBind(nb::module_& m)
        {
            audio(m);
            audioResample(m);
            hdr(m);
            time(m);
            url(m);
        }
    }
}

