// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/TimelineOptions.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void timelineOptions(py::module_& m)
        {
            py::enum_<ImageSeqAudio>(m, "ImageSeqAudio")
                .value("None", ImageSeqAudio::None)
                .value("Ext", ImageSeqAudio::Ext)
                .value("FileName", ImageSeqAudio::FileName);
            FTK_ENUM_BIND(m, ImageSeqAudio);

            py::class_<Options>(m, "Options")
                .def(py::init())
                .def_readwrite("imageSeqAudio", &Options::imageSeqAudio)
                .def_readwrite("imageSeqAudioExts", &Options::imageSeqAudioExts)
                .def_readwrite("imageSeqAudioFileName", &Options::imageSeqAudioFileName)
                .def_readwrite("compat", &Options::compat)
                .def_readwrite("threaded", &Options::threaded)
                .def_readwrite("readThreadCount", &Options::readThreadCount)
                .def_readwrite("audioRequestMax", &Options::audioRequestMax)
                .def_readwrite("ioOptions", &Options::ioOptions)
                .def_readwrite("pathOptions", &Options::pathOptions)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
        }
    }
}
