// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>

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
        void util(nb::module_& m)
        {
            m.def(
                "getExts",
                &getExts,
                nb::arg("context"),
                nb::arg("types") =
                    static_cast<int>(FileType::Media) |
                    static_cast<int>(FileType::Seq) |
                    static_cast<int>(FileType::Audio));

            m.def(
                "getPaths",
                &getPaths,
                nb::arg("context"),
                nb::arg("path"),
                nb::arg("options") = ftk::DirListOptions(),
                "Get a list of paths to open from the given path.");
        }
    }
}
