// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/BackgroundOptions.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void backgroundOptions(py::module_& m)
        {
            py::enum_<Background>(m, "Background")
                .value("Solid", Background::Solid)
                .value("Checkers", Background::Checkers)
                .value("Gradient", Background::Gradient);
            FTK_ENUM_BIND(m, Background);

            py::class_<Outline>(m, "Outline")
                .def(py::init())
                .def_readwrite("enabled", &Outline::enabled)
                .def_readwrite("width", &Outline::width)
                .def_readwrite("color", &Outline::color)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<BackgroundOptions>(m, "BackgroundOptions")
                .def(py::init())
                .def_readwrite("type", &BackgroundOptions::type)
                .def_readwrite("solidColor", &BackgroundOptions::solidColor)
                .def_readwrite("checkersColor", &BackgroundOptions::checkersColor)
                .def_readwrite("checkersSize", &BackgroundOptions::checkersSize)
                .def_readwrite("gradientColor", &BackgroundOptions::gradientColor)
                .def_readwrite("outline", &BackgroundOptions::outline)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            m.def("to_json",
                [](const Background& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const Outline& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const BackgroundOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, Background& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, Outline& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, BackgroundOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
