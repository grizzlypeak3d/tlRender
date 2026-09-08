// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/ColorOptions.h>

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
        void colorOptions(nb::module_& m)
        {
            nb::enum_<OCIOConfig>(m, "OCIOConfig")
                .value("BuiltIn", OCIOConfig::BuiltIn)
                .value("EnvVar", OCIOConfig::EnvVar)
                .value("File", OCIOConfig::File);
            FTK_ENUM_BIND(m, OCIOConfig);

            nb::class_<OCIOOptions>(m, "OCIOOptions")
                .def(nb::init())
                .def_rw("enabled", &OCIOOptions::enabled)
                .def_rw("config", &OCIOOptions::config)
                .def_rw("fileName", &OCIOOptions::fileName)
                .def_rw("input", &OCIOOptions::input)
                .def_rw("display", &OCIOOptions::display)
                .def_rw("view", &OCIOOptions::view)
                .def_rw("look", &OCIOOptions::look)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::enum_<LUTDirection>(m, "LUTDirection")
                .value("Forward", LUTDirection::Forward)
                .value("Inverse", LUTDirection::Inverse);
            FTK_ENUM_BIND(m, LUTDirection);

            nb::enum_<LUTOrder>(m, "LUTOrder")
                .value("PostConfig", LUTOrder::PostConfig)
                .value("PreConfig", LUTOrder::PreConfig);
            FTK_ENUM_BIND(m, LUTOrder);

            nb::class_<LUTOptions>(m, "LUTOptions")
                .def(nb::init())
                .def_rw("enabled", &LUTOptions::enabled)
                .def_rw("fileName", &LUTOptions::fileName)
                .def_rw("direction", &LUTOptions::direction)
                .def_rw("order", &LUTOptions::order)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

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
