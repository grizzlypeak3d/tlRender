// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/BackgroundOptions.h>

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
        void backgroundOptions(nb::module_& m)
        {
            FTK_ENUM_PY(m, Background);
            FTK_ENUM_BIND(m, Background);

            nb::class_<Outline>(m, "Outline")
                .def(nb::init())
                .def_rw("enabled", &Outline::enabled)
                .def_rw("width", &Outline::width)
                .def_rw("color", &Outline::color)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<BackgroundOptions>(m, "BackgroundOptions")
                .def(nb::init())
                .def_rw("type", &BackgroundOptions::type)
                .def_rw("solidColor", &BackgroundOptions::solidColor)
                .def_rw("checkersColor", &BackgroundOptions::checkersColor)
                .def_rw("checkersSize", &BackgroundOptions::checkersSize)
                .def_rw("gradientColor", &BackgroundOptions::gradientColor)
                .def_rw("outline", &BackgroundOptions::outline)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

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
