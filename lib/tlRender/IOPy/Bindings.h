// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        void io(pybind11::module_&);
        void plugin(pybind11::module_&);
        void read(pybind11::module_&);
        void ioSystem(pybind11::module_&);
        void write(pybind11::module_&);

        void ioBind(pybind11::module_&);
    }
}
