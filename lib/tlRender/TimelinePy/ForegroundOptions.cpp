// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/ForegroundOptions.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void foregroundOptions(py::module_& m)
        {
            py::enum_<GridCellMode>(m, "GridCellMode")
                .value("CellSize", GridCellMode::CellSize)
                .value("CellCount", GridCellMode::CellCount);
            FTK_ENUM_BIND(m, GridCellMode);

            py::class_<Grid>(m, "Grid")
                .def(py::init())
                .def_readwrite("enabled", &Grid::enabled)
                .def_readwrite("cellMode", &Grid::cellMode)
                .def_readwrite("cellSize", &Grid::cellSize)
                .def_readwrite("cellCount", &Grid::cellCount)
                .def_readwrite("lineWidth", &Grid::lineWidth)
                .def_readwrite("color", &Grid::color)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::class_<ForegroundOptions>(m, "ForegroundOptions")
                .def(py::init())
                .def_readwrite("grid", &ForegroundOptions::grid)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

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
