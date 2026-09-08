// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/AudioSystem.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

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
        void audioSystem(nb::module_& m)
        {
            nb::class_<AudioDeviceID>(m, "AudioDeviceID")
                .def(nb::init())
                .def_rw("number", &AudioDeviceID::number)
                .def_rw("name", &AudioDeviceID::name)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<AudioDeviceInfo>(m, "AudioDeviceInfo")
                .def(nb::init())
                .def_rw("id", &AudioDeviceInfo::id)
                .def_rw("info", &AudioDeviceInfo::info)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            ftk::python::observable<AudioDeviceInfo>(m, "AudioDeviceInfo");
            ftk::python::observableList<AudioDeviceInfo>(m, "AudioDeviceInfo");

            nb::class_<AudioSystem, ftk::ISystem>(m, "AudioSystem")
                .def(nb::new_(&AudioSystem::create),
                    nb::arg("context"))
                .def_prop_ro("drivers", &AudioSystem::getDrivers)
                .def_prop_ro("currentDriver", &AudioSystem::getCurrentDriver)
                .def_prop_ro("devices", &AudioSystem::getDevices)
                .def_prop_ro("observeDevices", &AudioSystem::observeDevices)
                .def_prop_ro("defaultDevice", &AudioSystem::getDefaultDevice)
                .def_prop_ro("observeDefaultDevice", &AudioSystem::observeDefaultDevice);
        }
    }
}
