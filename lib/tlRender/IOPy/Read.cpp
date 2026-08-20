// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Read.h>

#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void read(py::module_& m)
        {
            py::class_<IRead, IIO, std::shared_ptr<IRead> >(m, "IRead")
                .def("cancelRequests", &IRead::cancelRequests)
                .def_property_readonly("error", &IRead::getError)
                .def_property_readonly("errorCount", &IRead::getErrorCount);

            py::class_<IVideoRead, IRead, std::shared_ptr<IVideoRead> >(m, "IVideoRead");

            py::class_<IAudioRead, IRead, std::shared_ptr<IAudioRead> >(m, "IAudioRead");

            py::class_<IReadPlugin, IIOPlugin, std::shared_ptr<IReadPlugin> >(m, "IReadPlugin")
                .def("videoRead", py::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&IReadPlugin::videoRead),
                    py::arg("path"),
                    py::arg("options") = IOOptions())
                .def("audioRead", py::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&IReadPlugin::audioRead),
                    py::arg("path"),
                    py::arg("options") = IOOptions());
        }
    }
}
