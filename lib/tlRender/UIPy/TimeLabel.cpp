// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/TimeLabel.h>

#include <tlRender/UI/TimeLabel.h>

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
#include <nanobind/operators.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void timeLabel(nb::module_& m)
        {
            using namespace ui;

            nb::class_<TimeLabel, ftk::IWidget>(m, "TimeLabel")
                .def(
                    nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<TimeUnitsModel>&,
                        const std::shared_ptr<ftk::IWidget>&>(&TimeLabel::create)),
                    nb::arg("context"),
                    nb::arg("timeUnitsModel"),
                    nb::arg("parent") = nullptr)
                .def_prop_rw("value", &TimeLabel::getValue, &TimeLabel::setValue)
                .def("setMarginRole", &TimeLabel::setMarginRole);
        }
    }
}

