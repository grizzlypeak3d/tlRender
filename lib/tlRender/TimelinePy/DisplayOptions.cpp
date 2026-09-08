// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/DisplayOptions.h>

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
        void displayOptions(nb::module_& m)
        {
            nb::class_<Color>(m, "Color")
                .def(nb::init())
                .def_rw("enabled", &Color::enabled)
                .def_rw("add", &Color::add)
                .def_rw("brightness", &Color::brightness)
                .def_rw("contrast", &Color::contrast)
                .def_rw("saturation", &Color::saturation)
                .def_rw("hue", &Color::hue)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
            
            m.def("color", &color);

            nb::class_<Levels>(m, "Levels")
                .def(nb::init())
                .def_rw("enabled", &Levels::enabled)
                .def_rw("inLow", &Levels::inLow)
                .def_rw("inHigh", &Levels::inHigh)
                .def_rw("gamma", &Levels::gamma)
                .def_rw("outLow", &Levels::outLow)
                .def_rw("outHigh", &Levels::outHigh)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<Exposure>(m, "Exposure")
                .def(nb::init())
                .def_rw("enabled", &Exposure::enabled)
                .def_rw("exposure", &Exposure::exposure)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<SoftClip>(m, "SoftClip")
                .def(nb::init())
                .def_rw("enabled", &SoftClip::enabled)
                .def_rw("value", &SoftClip::value)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<AspectRatio>(m, "AspectRatio")
                .def(nb::init())
                .def(
                    nb::init<float, float>(),
                    nb::arg("num"),
                    nb::arg("den") = 1.F)
                .def_rw("num", &AspectRatio::num)
                .def_rw("den", &AspectRatio::den)
                .def("isValid", &AspectRatio::isValid)
                .def("__float__", [](const AspectRatio& value)
                    {
                        return static_cast<float>(value);
                    })
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def("getLabel", [](const AspectRatio& value)
                {
                    return getLabel(value);
                });

            FTK_ENUM_PY(m, AspectRatioType);
            FTK_ENUM_BIND(m, AspectRatioType);

            nb::class_<AspectRatioOptions>(m, "AspectRatioOptions")
                .def(nb::init())
                .def(
                    nb::init<const AspectRatio&, AspectRatioType>(),
                    nb::arg("value"),
                    nb::arg("type"))
                .def_rw("value", &AspectRatioOptions::value)
                .def_rw("type", &AspectRatioOptions::type)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def("getLabel", [](const AspectRatioOptions& value)
                {
                    return getLabel(value);
                });

            nb::class_<DisplayOptions>(m, "DisplayOptions")
                .def(nb::init())
                .def_rw("channels", &DisplayOptions::channels)
                .def_rw("negative", &DisplayOptions::negative)
                .def_rw("mirror", &DisplayOptions::mirror)
                .def_rw("color", &DisplayOptions::color)
                .def_rw("levels", &DisplayOptions::levels)
                .def_rw("exposure", &DisplayOptions::exposure)
                .def_rw("softClip", &DisplayOptions::softClip)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

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
