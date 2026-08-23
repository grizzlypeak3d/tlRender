// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        void frameToolBar(pybind11::module_&);
        void playbackToolBar(pybind11::module_&);

        void uiBind(pybind11::module_&);
    }
}

