// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <nanobind/nanobind.h>

namespace tl
{
    namespace python
    {
        void frameToolBar(nanobind::module_&);
        void playbackToolBar(nanobind::module_&);

        void uiBind(nanobind::module_&);
    }
}

