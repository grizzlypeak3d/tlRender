// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        TL_API void audio(pybind11::module_&);
        TL_API void audioResample(pybind11::module_&);
        TL_API void hdr(pybind11::module_&);
        TL_API void time(pybind11::module_&);
        TL_API void url(pybind11::module_&);

        TL_API void coreBind(pybind11::module_&);
    }
}
