// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

namespace tl
{
    namespace python
    {
        void audio(nanobind::module_&);
        void audioResample(nanobind::module_&);
        void hdr(nanobind::module_&);
        void time(nanobind::module_&);
        void url(nanobind::module_&);

        void coreBind(nanobind::module_&);
    }
}
