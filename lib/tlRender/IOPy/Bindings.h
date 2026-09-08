// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <nanobind/nanobind.h>

namespace tl
{
    namespace python
    {
        void io(nanobind::module_&);
        void plugin(nanobind::module_&);
        void seqIO(nanobind::module_&);
        void read(nanobind::module_&);
        void ioSystem(nanobind::module_&);
        void write(nanobind::module_&);

        void ioBind(nanobind::module_&);
    }
}
