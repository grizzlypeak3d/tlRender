// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/System.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void ioSystem(py::module_& m)
        {
            py::class_<ReadSystem, ftk::ISystem, std::shared_ptr<ReadSystem> >(m, "ReadSystem")
                .def(py::init(&ReadSystem::create),
                    py::arg("context"))
                .def_property_readonly("plugins", &ReadSystem::getPlugins)
                .def("addPlugin", &ReadSystem::addPlugin,
                    py::arg("plugin"))
                .def("removePlugin", &ReadSystem::removePlugin,
                    py::arg("plugin"))
                .def("getPlugin", static_cast<std::shared_ptr<IReadPlugin>
                        (ReadSystem::*)(const ftk::Path&) const>(&ReadSystem::getPlugin),
                    py::arg("path"))
                .def_property_readonly("names", &ReadSystem::getNames)
                .def("getExts", &ReadSystem::getExts,
                    py::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq))
                .def("getFileType", &ReadSystem::getFileType,
                    py::arg("extension"))
                .def("videoRead", py::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&ReadSystem::videoRead),
                    py::arg("path"),
                    py::arg("options") = IOOptions())
                .def("audioRead", py::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&ReadSystem::audioRead),
                    py::arg("path"),
                    py::arg("options") = IOOptions());

            py::class_<WriteSystem, ftk::ISystem, std::shared_ptr<WriteSystem> >(m, "WriteSystem")
                .def(py::init(&WriteSystem::create),
                    py::arg("context"))
                .def_property_readonly("plugins", &WriteSystem::getPlugins)
                .def("addPlugin", &WriteSystem::addPlugin,
                    py::arg("plugin"))
                .def("removePlugin", &WriteSystem::removePlugin,
                    py::arg("plugin"))
                .def("getPlugin", static_cast<std::shared_ptr<IWritePlugin>
                        (WriteSystem::*)(const ftk::Path&) const>(&WriteSystem::getPlugin),
                    py::arg("path"))
                .def_property_readonly("names", &WriteSystem::getNames)
                .def("getExts", &WriteSystem::getExts,
                    py::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq))
                .def("getFileType", &WriteSystem::getFileType,
                    py::arg("extension"))
                .def("write", &WriteSystem::write,
                    py::arg("path"),
                    py::arg("info"),
                    py::arg("options") = IOOptions());
        }
    }
}
