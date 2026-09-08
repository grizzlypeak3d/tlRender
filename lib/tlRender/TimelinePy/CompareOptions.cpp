// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/CompareOptions.h>

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
        void compareOptions(nb::module_& m)
        {
            FTK_ENUM_PY(m, Compare);
            FTK_ENUM_BIND(m, Compare);

            FTK_ENUM_PY(m, CompareTime);
            FTK_ENUM_BIND(m, CompareTime);

            nb::class_<CompareOptions>(m, "CompareOptions")
                .def(nb::init())
                .def_rw("compare", &CompareOptions::compare)
                .def_rw("wipeCenter", &CompareOptions::wipeCenter)
                .def_rw("wipeRotation", &CompareOptions::wipeRotation)
                .def_rw("overlay", &CompareOptions::overlay)
                .def_rw("differenceGain", &CompareOptions::differenceGain)
                .def_rw("sameSize", &CompareOptions::sameSize)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def("to_json",
                [](const CompareOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, CompareOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
