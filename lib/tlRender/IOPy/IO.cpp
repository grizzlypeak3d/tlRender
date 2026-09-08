// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/IO.h>

#include <nanobind/operators.h>
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
        void io(nb::module_& m)
        {
            nb::enum_<FileType>(
                m, "FileType",
                // A bitmask: the Python code builds int flag combinations.
                nb::is_arithmetic(), nb::is_flag())
                .value("Unknown", FileType::Unknown)
                .value("Media", FileType::Media)
                .value("Seq", FileType::Seq)
                .value("Audio", FileType::Audio);
            
            nb::class_<IOInfo>(m, "IOInfo")
                .def(nb::init())
                .def_rw("video", &IOInfo::video)
                .def_rw("videoTime", &IOInfo::videoTime)
                .def_rw("audio", &IOInfo::audio)
                .def_rw("audioTime", &IOInfo::audioTime)
                .def_rw("tags", &IOInfo::tags)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
        }
    }
}
