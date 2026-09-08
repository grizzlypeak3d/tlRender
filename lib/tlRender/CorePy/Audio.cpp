// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/Audio.h>

#include <ftk/CorePy/Bindings.h>

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
        void audio(nb::module_& m)
        {
            FTK_ENUM_PY(m, AudioType);
            FTK_ENUM_BIND(m, AudioType);
            
            m.def("getByteCount", &getByteCount);
            m.def("getIntAudioType", &getIntAudioType);
            m.def("getFloatAudioType", &getFloatAudioType);
            
            nb::class_<AudioInfo>(m, "AudioInfo")
                .def(nb::init())
                .def_rw("name", &AudioInfo::name)
                .def_rw("channelCount", &AudioInfo::channelCount)
                .def_rw("type", &AudioInfo::type)
                .def_rw("sampleRate", &AudioInfo::sampleRate)
                .def_prop_ro("isValid", &AudioInfo::isValid)
                .def_prop_ro("byteCount", &AudioInfo::getByteCount)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
            
            nb::class_<Audio>(m, "Audio")
                .def(nb::new_(&Audio::create),
                    nb::arg("info"),
                    nb::arg("sampleCount"))
                .def_prop_ro("info", &Audio::getInfo, nb::rv_policy::copy)
                .def_prop_ro("channelCount", &Audio::getChannelCount)
                .def_prop_ro("type", &Audio::getType)
                .def_prop_ro("sampleRate", &Audio::getSampleRate)
                .def_prop_ro("sampleCount", &Audio::getSampleCount)
                .def_prop_ro("isValid", &Audio::isValid)
                .def_prop_ro("byteCount", &Audio::getByteCount)
                .def("zero", &Audio::zero);
        }
    }
}
