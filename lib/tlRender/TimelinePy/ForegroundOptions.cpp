// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/ForegroundOptions.h>

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
        void foregroundOptions(nb::module_& m)
        {
            nb::enum_<GridCellMode>(m, "GridCellMode")
                .value("CellSize", GridCellMode::CellSize)
                .value("CellCount", GridCellMode::CellCount);
            FTK_ENUM_BIND(m, GridCellMode);

            nb::class_<Grid>(m, "Grid")
                .def(nb::init())
                .def_rw("enabled", &Grid::enabled)
                .def_rw("cellMode", &Grid::cellMode)
                .def_rw("cellSize", &Grid::cellSize)
                .def_rw("cellCount", &Grid::cellCount)
                .def_rw("lineWidth", &Grid::lineWidth)
                .def_rw("color", &Grid::color)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<CenterMarker>(m, "CenterMarker")
                .def(nb::init())
                .def_rw("enabled", &CenterMarker::enabled)
                .def_rw("size", &CenterMarker::size)
                .def_rw("width", &CenterMarker::width)
                .def_rw("color", &CenterMarker::color)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<MissingIndicator>(m, "MissingIndicator")
                .def(nb::init())
                .def_rw("enabled", &MissingIndicator::enabled)
                .def_rw("width", &MissingIndicator::width)
                .def_rw("color", &MissingIndicator::color)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<ForegroundOptions>(m, "ForegroundOptions")
                .def(nb::init())
                .def_rw("grid", &ForegroundOptions::grid)
                .def_rw("centerMarker", &ForegroundOptions::centerMarker)
                .def_rw("missingIndicator", &ForegroundOptions::missingIndicator)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def("to_json",
                [](const Grid& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });
            m.def("to_json",
                [](const ForegroundOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, Grid& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
            m.def("from_json",
                [](const std::string& value, ForegroundOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}
