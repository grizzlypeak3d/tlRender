// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/ItemOptions.h>

#include <tlRender/UI/ItemOptions.h>

#include <ftk/CorePy/Bindings.h>

#include <ftk/Core/Context.h>

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void itemOptions(py::module_& m)
        {
            using namespace ui;
            
            py::enum_<InOutDisplay>(m, "InOutDisplay")
                .value("InsideRange", InOutDisplay::InsideRange)
                .value("OutsideRange", InOutDisplay::OutsideRange);
            FTK_ENUM_BIND(m, InOutDisplay);

            py::enum_<CacheDisplay>(m, "CacheDisplay")
                .value("VideoAndAudio", CacheDisplay::VideoAndAudio)
                .value("VideoOnly", CacheDisplay::VideoOnly);
            FTK_ENUM_BIND(m, CacheDisplay);

            py::enum_<WaveformPrim>(m, "WaveformPrim")
                .value("Mesh", WaveformPrim::Mesh)
                .value("Image", WaveformPrim::Image);
            FTK_ENUM_BIND(m, WaveformPrim);

            py::class_<ItemData, std::shared_ptr<ItemData> >(m, "ItemData")
                .def(py::init())
                .def_readwrite("speed", &ItemData::speed)
                .def_readwrite("dir", &ItemData::dir)
                .def_readwrite("options", &ItemData::options)
                .def_readwrite("timeUnitsModel", &ItemData::timeUnitsModel)
                .def_readwrite("toMediaTime", &ItemData::toMediaTime);

            py::class_<ItemOptions>(m, "ItemOptions")
                .def(py::init())
                .def_readwrite("inputEnabled", &ItemOptions::inputEnabled)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<DisplayOptions>(m, "DisplayOptions")
                .def(py::init())
                .def_readwrite("inOutDisplay", &DisplayOptions::inOutDisplay)
                .def_readwrite("cacheDisplay", &DisplayOptions::cacheDisplay)
                .def_readwrite("minimize", &DisplayOptions::minimize)
                .def_readwrite("clipColors", &DisplayOptions::clipColors)
                .def_readwrite("thumbnails", &DisplayOptions::thumbnails)
                .def_readwrite("thumbnailHeight", &DisplayOptions::thumbnailHeight)
                .def_readwrite("waveforms", &DisplayOptions::waveforms)
                .def_readwrite("waveformWidth", &DisplayOptions::waveformWidth)
                .def_readwrite("waveformHeight", &DisplayOptions::waveformHeight)
                .def_readwrite("waveformPrim", &DisplayOptions::waveformPrim)
                .def_readwrite("clipRectScale", &DisplayOptions::clipRectScale)
                .def_readwrite("ocio", &DisplayOptions::ocio)
                .def_readwrite("lut", &DisplayOptions::lut)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
            
        }
    }
}

