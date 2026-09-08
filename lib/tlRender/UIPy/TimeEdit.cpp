// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/TimeEdit.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Context.h>

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
#include <nanobind/stl/function.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void timeEdit(nb::module_& m)
        {
            using namespace ui;

            nb::class_<TimeMap>(m, "TimeMap")
                .def(nb::init())
                .def_rw("toMedia", &TimeMap::toMedia)
                .def_rw("fromMedia", &TimeMap::fromMedia);

            nb::class_<TimeEdit, ftk::IWidget>(m, "TimeEdit")
                .def(
                    nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<TimeUnitsModel>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimeEdit::create)),
                    nb::arg("context"),
                    nb::arg("timeUnitsModel"),
                    nb::arg("parent") = nullptr)
                .def_prop_rw("value", &TimeEdit::getValue, &TimeEdit::setValue)
                .def("setCallback", &TimeEdit::setCallback)
                .def("setTimeMap", &TimeEdit::setTimeMap)
                .def("selectAll", &TimeEdit::selectAll);
        }
    }
}

