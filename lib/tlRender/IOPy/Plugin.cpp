// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Plugin.h>

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
        void plugin(nb::module_& m)
        {
            nb::class_<IIO>(m, "IIO")
                .def_prop_ro("path", &IIO::getPath, nb::rv_policy::copy)
                .def_static("getObjectCount", &IIO::getObjectCount);

            nb::class_<IIOPlugin>(m, "IIOPlugin")
                .def_prop_ro("pluginName", &IIOPlugin::getPluginName)
                .def("getPluginInfo", &IIOPlugin::getPluginInfo,
                    nb::arg("options") = IOOptions())
                .def("getExts", &IIOPlugin::getExts,
                    nb::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq) |
                        static_cast<int>(FileType::Audio));
        }
    }
}
