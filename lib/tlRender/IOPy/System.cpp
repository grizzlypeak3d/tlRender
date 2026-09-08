// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/System.h>

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
        void ioSystem(nb::module_& m)
        {
            nb::class_<ReadSystem, ftk::ISystem>(m, "ReadSystem")
                .def(nb::new_(&ReadSystem::create),
                    nb::arg("context"))
                .def_prop_ro("plugins", &ReadSystem::getPlugins)
                .def("addPlugin", &ReadSystem::addPlugin,
                    nb::arg("plugin"))
                .def("removePlugin", &ReadSystem::removePlugin,
                    nb::arg("plugin"))
                .def("getPlugin", static_cast<std::shared_ptr<IReadPlugin>
                        (ReadSystem::*)(const ftk::Path&) const>(&ReadSystem::getPlugin),
                    nb::arg("path"))
                .def_prop_ro("names", &ReadSystem::getNames)
                .def("getExts", &ReadSystem::getExts,
                    nb::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq))
                .def("getFileType", &ReadSystem::getFileType,
                    nb::arg("extension"))
                .def("videoRead", nb::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&ReadSystem::videoRead),
                    nb::arg("path"),
                    nb::arg("options") = IOOptions())
                .def("audioRead", nb::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&ReadSystem::audioRead),
                    nb::arg("path"),
                    nb::arg("options") = IOOptions());

            nb::class_<WriteSystem, ftk::ISystem>(m, "WriteSystem")
                .def(nb::new_(&WriteSystem::create),
                    nb::arg("context"))
                .def_prop_ro("plugins", &WriteSystem::getPlugins)
                .def("addPlugin", &WriteSystem::addPlugin,
                    nb::arg("plugin"))
                .def("removePlugin", &WriteSystem::removePlugin,
                    nb::arg("plugin"))
                .def("getPlugin", static_cast<std::shared_ptr<IWritePlugin>
                        (WriteSystem::*)(const ftk::Path&) const>(&WriteSystem::getPlugin),
                    nb::arg("path"))
                .def_prop_ro("names", &WriteSystem::getNames)
                .def("getExts", &WriteSystem::getExts,
                    nb::arg("types") =
                        static_cast<int>(FileType::Media) |
                        static_cast<int>(FileType::Seq))
                .def("getFileType", &WriteSystem::getFileType,
                    nb::arg("extension"))
                .def("write", &WriteSystem::write,
                    nb::arg("path"),
                    nb::arg("info"),
                    nb::arg("options") = IOOptions());
        }
    }
}
