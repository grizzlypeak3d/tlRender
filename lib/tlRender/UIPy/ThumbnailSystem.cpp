// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/ThumbnailSystem.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void thumbnailSystem(py::module_& m)
        {
            using namespace ui;

            // Only the cache management; the request API is futures, which
            // needs a design of its own before it is bound.
            py::class_<ThumbnailSystem, ftk::ISystem, std::shared_ptr<ThumbnailSystem> >(m, "ThumbnailSystem")
                .def("clearCache", &ThumbnailSystem::clearCache);
        }
    }
}
