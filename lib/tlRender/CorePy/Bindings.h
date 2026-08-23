// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        void audio(pybind11::module_&);
        void audioResample(pybind11::module_&);
        void hdr(pybind11::module_&);
        void time(pybind11::module_&);
        void url(pybind11::module_&);

        void coreBind(pybind11::module_&);
    }
}
