// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/AudioSystem.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void audioSystem(py::module_& m)
        {
            py::class_<AudioDeviceID>(m, "AudioDeviceID")
                .def(py::init())
                .def_readwrite("number", &AudioDeviceID::number)
                .def_readwrite("name", &AudioDeviceID::name)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<AudioDeviceInfo>(m, "AudioDeviceInfo")
                .def(py::init())
                .def_readwrite("id", &AudioDeviceInfo::id)
                .def_readwrite("info", &AudioDeviceInfo::info)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            ftk::python::observable<AudioDeviceInfo>(m, "AudioDeviceInfo");
            ftk::python::observableList<AudioDeviceInfo>(m, "AudioDeviceInfo");

            py::class_<AudioSystem, ftk::ISystem, std::shared_ptr<AudioSystem> >(m, "AudioSystem")
                .def(py::init(&AudioSystem::create),
                    py::arg("context"))
                .def_property_readonly("drivers", &AudioSystem::getDrivers)
                .def_property_readonly("currentDriver", &AudioSystem::getCurrentDriver)
                .def_property_readonly("devices", &AudioSystem::getDevices)
                .def_property_readonly("observeDevices", &AudioSystem::observeDevices)
                .def_property_readonly("defaultDevice", &AudioSystem::getDefaultDevice)
                .def_property_readonly("observeDefaultDevice", &AudioSystem::observeDefaultDevice);
        }
    }
}
