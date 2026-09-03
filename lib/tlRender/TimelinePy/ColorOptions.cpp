// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/ColorOptions.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void colorOptions(py::module_& m)
        {
            py::enum_<OCIOConfig>(m, "OCIOConfig")
                .value("BuiltIn", OCIOConfig::BuiltIn)
                .value("EnvVar", OCIOConfig::EnvVar)
                .value("File", OCIOConfig::File);
            FTK_ENUM_BIND(m, OCIOConfig);

            py::class_<OCIOOptions>(m, "OCIOOptions")
                .def(py::init())
                .def_readwrite("enabled", &OCIOOptions::enabled)
                .def_readwrite("config", &OCIOOptions::config)
                .def_readwrite("fileName", &OCIOOptions::fileName)
                .def_readwrite("input", &OCIOOptions::input)
                .def_readwrite("display", &OCIOOptions::display)
                .def_readwrite("view", &OCIOOptions::view)
                .def_readwrite("look", &OCIOOptions::look)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::enum_<LUTDirection>(m, "LUTDirection")
                .value("Forward", LUTDirection::Forward)
                .value("Inverse", LUTDirection::Inverse);
            FTK_ENUM_BIND(m, LUTDirection);

            py::enum_<LUTOrder>(m, "LUTOrder")
                .value("PostConfig", LUTOrder::PostConfig)
                .value("PreConfig", LUTOrder::PreConfig);
            FTK_ENUM_BIND(m, LUTOrder);

            py::class_<LUTOptions>(m, "LUTOptions")
                .def(py::init())
                .def_readwrite("enabled", &LUTOptions::enabled)
                .def_readwrite("fileName", &LUTOptions::fileName)
                .def_readwrite("direction", &LUTOptions::direction)
                .def_readwrite("order", &LUTOptions::order)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            m.def("getLUTFormatNames", &getLUTFormatNames);
            m.def("getLUTFormatExts", &getLUTFormatExts);

            m.def("to_json",
                [](const OCIOOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const LUTOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, OCIOOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, LUTOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
