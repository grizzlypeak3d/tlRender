// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/DisplayOptions.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void displayOptions(py::module_& m)
        {
            py::class_<Color>(m, "Color")
                .def(py::init())
                .def_readwrite("enabled", &Color::enabled)
                .def_readwrite("add", &Color::add)
                .def_readwrite("brightness", &Color::brightness)
                .def_readwrite("contrast", &Color::contrast)
                .def_readwrite("saturation", &Color::saturation)
                .def_readwrite("hue", &Color::hue)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
            
            m.def("color", &color);

            py::class_<Levels>(m, "Levels")
                .def(py::init())
                .def_readwrite("enabled", &Levels::enabled)
                .def_readwrite("inLow", &Levels::inLow)
                .def_readwrite("inHigh", &Levels::inHigh)
                .def_readwrite("gamma", &Levels::gamma)
                .def_readwrite("outLow", &Levels::outLow)
                .def_readwrite("outHigh", &Levels::outHigh)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<Exposure>(m, "Exposure")
                .def(py::init())
                .def_readwrite("enabled", &Exposure::enabled)
                .def_readwrite("exposure", &Exposure::exposure)
                .def_readwrite("defog", &Exposure::defog)
                .def_readwrite("kneeLow", &Exposure::kneeLow)
                .def_readwrite("kneeHigh", &Exposure::kneeHigh)
                .def_readwrite("gamma", &Exposure::gamma)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<SoftClip>(m, "SoftClip")
                .def(py::init())
                .def_readwrite("enabled", &SoftClip::enabled)
                .def_readwrite("value", &SoftClip::value)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<DisplayOptions>(m, "DisplayOptions")
                .def(py::init())
                .def_readwrite("channels", &DisplayOptions::channels)
                .def_readwrite("negative", &DisplayOptions::negative)
                .def_readwrite("mirror", &DisplayOptions::mirror)
                .def_readwrite("color", &DisplayOptions::color)
                .def_readwrite("levels", &DisplayOptions::levels)
                .def_readwrite("exposure", &DisplayOptions::exposure)
                .def_readwrite("softClip", &DisplayOptions::softClip)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            m.def("to_json",
                [](const Color& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const Levels& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const Exposure& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const SoftClip& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const DisplayOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, Color& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, Levels& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, Exposure& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, SoftClip& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, DisplayOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
