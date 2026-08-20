// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Plugin.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void plugin(py::module_& m)
        {
            py::class_<IIO, std::shared_ptr<IIO> >(m, "IIO")
                .def_property_readonly("path", &IIO::getPath, py::return_value_policy::copy)
                .def_static("getObjectCount", &IIO::getObjectCount);

            py::class_<IIOPlugin, std::shared_ptr<IIOPlugin> >(m, "IIOPlugin")
                .def_property_readonly("pluginName", &IIOPlugin::getPluginName)
                .def("getPluginInfo", &IIOPlugin::getPluginInfo,
                    py::arg("options") = IOOptions())
                .def("getExts", &IIOPlugin::getExts,
                    py::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq) |
                        static_cast<int>(FileType::Audio));
        }
    }
}
