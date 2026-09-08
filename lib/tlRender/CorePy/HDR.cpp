// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/HDR.h>

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
        void hdr(nb::module_& m)
        {
            FTK_ENUM_PY(m, HDR_EOTF);
            FTK_ENUM_BIND(m, HDR_EOTF);

            FTK_ENUM_PY(m, HDRPrimaries);
            FTK_ENUM_BIND(m, HDRPrimaries);

            nb::class_<HDRData>(m, "HDRData")
                .def(nb::init())
                .def_rw("eotf", &HDRData::eotf)
                .def_rw("primaries", &HDRData::primaries)
                .def_rw("displayMasteringLuminance", &HDRData::displayMasteringLuminance)
                .def_rw("maxCLL", &HDRData::maxCLL)
                .def_rw("maxFALL", &HDRData::maxFALL)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
        }
    }
}
