// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/ThumbnailSystem.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <ftk/Core/Context.h>

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
        void thumbnailSystem(nb::module_& m)
        {
            using namespace ui;

            // Only the cache management; the request API is futures, which
            // needs a design of its own before it is bound.
            nb::class_<ThumbnailSystem, ftk::ISystem>(m, "ThumbnailSystem")
                .def("clearCache", &ThumbnailSystem::clearCache);
        }
    }
}
