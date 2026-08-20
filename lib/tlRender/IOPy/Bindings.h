// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        TL_API void io(pybind11::module_&);
        TL_API void plugin(pybind11::module_&);
        TL_API void read(pybind11::module_&);
        TL_API void ioSystem(pybind11::module_&);
        TL_API void write(pybind11::module_&);

        TL_API void ioBind(pybind11::module_&);
    }
}
