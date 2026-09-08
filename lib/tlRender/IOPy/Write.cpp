// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Write.h>

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
        void write(nb::module_& m)
        {
            nb::class_<IWrite, IIO>(m, "IWrite")
                .def("writeVideo", &IWrite::writeVideo,
                    nb::arg("time"),
                    nb::arg("image"),
                    nb::arg("options") = IOOptions())
                .def("writeAudio", &IWrite::writeAudio,
                    nb::arg("timeRange"),
                    nb::arg("audio"),
                    nb::arg("options") = IOOptions())
                .def("finish", &IWrite::finish);

            nb::class_<IWritePlugin, IIOPlugin>(m, "IWritePlugin")
                .def("getInfo", &IWritePlugin::getInfo,
                    nb::arg("info"),
                    nb::arg("options") = IOOptions())
                .def("write", &IWritePlugin::write,
                    nb::arg("path"),
                    nb::arg("info"),
                    nb::arg("options") = IOOptions());
        }
    }
}
