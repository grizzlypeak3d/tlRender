// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/IO.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void io(py::module_& m)
        {
            py::enum_<FileType>(m, "FileType")
                .value("Unknown", FileType::Unknown)
                .value("Media", FileType::Media)
                .value("Seq", FileType::Seq)
                .value("Audio", FileType::Audio);
            
            py::class_<IOInfo>(m, "IOInfo")
                .def(py::init())
                .def_readwrite("video", &IOInfo::video)
                .def_readwrite("videoTime", &IOInfo::videoTime)
                .def_readwrite("audio", &IOInfo::audio)
                .def_readwrite("audioTime", &IOInfo::audioTime)
                .def_readwrite("tags", &IOInfo::tags)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
        }
    }
}
