// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/Read.h>

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
        void read(nb::module_& m)
        {
            nb::class_<IRead, IIO>(m, "IRead")
                .def("cancelRequests", &IRead::cancelRequests)
                .def_prop_ro("error", &IRead::getError)
                .def_prop_ro("errorCount", &IRead::getErrorCount);

            nb::class_<IVideoRead, IRead>(m, "IVideoRead");

            nb::class_<IAudioRead, IRead>(m, "IAudioRead");

            nb::class_<IReadPlugin, IIOPlugin>(m, "IReadPlugin")
                .def("videoRead", nb::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&IReadPlugin::videoRead),
                    nb::arg("path"),
                    nb::arg("options") = IOOptions())
                .def("audioRead", nb::overload_cast<
                        const ftk::Path&,
                        const IOOptions&>(&IReadPlugin::audioRead),
                    nb::arg("path"),
                    nb::arg("options") = IOOptions());
        }
    }
}
