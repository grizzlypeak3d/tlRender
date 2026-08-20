// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Write.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void write(py::module_& m)
        {
            py::class_<IWrite, IIO, std::shared_ptr<IWrite> >(m, "IWrite")
                .def("writeVideo", &IWrite::writeVideo,
                    py::arg("time"),
                    py::arg("image"),
                    py::arg("options") = IOOptions())
                .def("writeAudio", &IWrite::writeAudio,
                    py::arg("timeRange"),
                    py::arg("audio"),
                    py::arg("options") = IOOptions())
                .def("finish", &IWrite::finish);

            py::class_<IWritePlugin, IIOPlugin, std::shared_ptr<IWritePlugin> >(m, "IWritePlugin")
                .def("getInfo", &IWritePlugin::getInfo,
                    py::arg("info"),
                    py::arg("options") = IOOptions())
                .def("write", &IWritePlugin::write,
                    py::arg("path"),
                    py::arg("info"),
                    py::arg("options") = IOOptions());
        }
    }
}
