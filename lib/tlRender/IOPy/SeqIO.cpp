// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/SeqIO.h>

#include <ftk/CorePy/Bindings.h>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/optional.h>
#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/array.h>
#include <nanobind/stl/set.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void seqIO(nb::module_& m)
        {
            FTK_ENUM_PY(m, MissingFrames);
            FTK_ENUM_BIND(m, MissingFrames);

            m.def("isStructural", &isStructural, nb::arg("missingFrames"));

            nb::class_<SeqOptions>(m, "SeqOptions")
                .def(nb::init())
                .def_rw("defaultSpeed", &SeqOptions::defaultSpeed)
                .def_rw("missingFrames", &SeqOptions::missingFrames)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
        }
    }
}
