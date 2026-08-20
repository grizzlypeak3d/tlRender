// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/Audio.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void audio(py::module_& m)
        {
            py::enum_<AudioType>(m, "AudioType")
                .value("_None", AudioType::None)
                .value("S8", AudioType::S8)
                .value("S16", AudioType::S16)
                .value("S32", AudioType::S32)
                .value("F32", AudioType::F32)
                .value("F64", AudioType::F64);
            FTK_ENUM_BIND(m, AudioType);
            
            m.def("getByteCount", &getByteCount);
            m.def("getIntAudioType", &getIntAudioType);
            m.def("getFloatAudioType", &getFloatAudioType);
            
            py::class_<AudioInfo>(m, "AudioInfo")
                .def(py::init())
                .def_readwrite("name", &AudioInfo::name)
                .def_readwrite("channelCount", &AudioInfo::channelCount)
                .def_readwrite("type", &AudioInfo::type)
                .def_readwrite("sampleRate", &AudioInfo::sampleRate)
                .def_property_readonly("isValid", &AudioInfo::isValid)
                .def_property_readonly("byteCount", &AudioInfo::getByteCount)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
            
            py::class_<Audio, std::shared_ptr<Audio> >(m, "Audio")
                .def(py::init(&Audio::create),
                    py::arg("info"),
                    py::arg("sampleCount"))
                .def_property_readonly("info", &Audio::getInfo, py::return_value_policy::copy)
                .def_property_readonly("channelCount", &Audio::getChannelCount)
                .def_property_readonly("type", &Audio::getType)
                .def_property_readonly("sampleRate", &Audio::getSampleRate)
                .def_property_readonly("sampleCount", &Audio::getSampleCount)
                .def_property_readonly("isValid", &Audio::isValid)
                .def_property_readonly("byteCount", &Audio::getByteCount)
                .def("zero", &Audio::zero);
        }
    }
}
