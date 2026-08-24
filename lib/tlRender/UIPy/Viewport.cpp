// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/Viewport.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <pybind11/functional.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void viewport(py::module_& m)
        {
            using namespace ui;

            ftk::python::observable<std::pair<ftk::V2I, double> >(m, "ViewPosAndZoom");
            ftk::python::observable<std::optional<ftk::V2I> >(m, "OptionalV2I");
            ftk::python::observable<std::optional<ftk::Color4F> >(m, "OptionalColor4F");
            
            py::class_<Viewport, ftk::IWidget, std::shared_ptr<Viewport> >(m, "Viewport")
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<ftk::IWidget>&>(&Viewport::create)),
                    py::arg("context"),
                    py::arg("parent") = nullptr)
                .def_property("compareOptions",
                    &Viewport::getCompareOptions,
                    &Viewport::setCompareOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeCompareOptions",
                    &Viewport::observeCompareOptions)
                .def_property(
                    "ocioOptions",
                    &Viewport::getOCIOOptions,
                    &Viewport::setOCIOOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeOCIOOptions",
                    &Viewport::observeOCIOOptions)
                .def_property(
                    "LUTOptions",
                    &Viewport::getLUTOptions,
                    &Viewport::setLUTOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeLUTOptions",
                    &Viewport::observeLUTOptions)
                .def_property(
                    "imageOptions",
                    &Viewport::getImageOptions,
                    &Viewport::setImageOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeImageOptions",
                    &Viewport::observeImageOptions)
                .def_property(
                    "displayOptions",
                    &Viewport::getDisplayOptions,
                    &Viewport::setDisplayOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeDisplayOptions",
                    &Viewport::observeDisplayOptions)
                .def_property(
                    "backgroundOptions",
                    &Viewport::getBackgroundOptions,
                    &Viewport::setBackgroundOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeBackgroundOptions",
                    &Viewport::observeBackgroundOptions)
                .def_property(
                    "foregroundOptions",
                    &Viewport::getForegroundOptions,
                    &Viewport::setForegroundOptions,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeForegroundOptions",
                    &Viewport::observeForegroundOptions)
                .def_property(
                    "colorBuffer",
                    &Viewport::getColorBuffer,
                    &Viewport::setColorBuffer)
                .def_property_readonly(
                    "observeColorBuffer",
                    &Viewport::observeColorBuffer)
                .def_property(
                    "player",
                    &Viewport::getPlayer,
                    &Viewport::setPlayer)
                .def_property_readonly(
                    "viewPos",
                    &Viewport::getViewPos,
                    py::return_value_policy::copy)
                .def_property_readonly(
                    "observeViewPos",
                    &Viewport::observeViewPos)
                .def_property_readonly(
                    "zoom",
                    &Viewport::getZoom)
                .def_property_readonly(
                    "observeZoom",
                    &Viewport::observeZoom)
                .def_property_readonly(
                    "viewPosAndZoom",
                    &Viewport::getViewPosAndZoom)
                .def(
                    "setViewPosAndZoom",
                    &Viewport::setViewPosAndZoom,
                    py::arg("pos"),
                    py::arg("zoom"))
                .def_property_readonly(
                    "observeViewPosAndZoom",
                    &Viewport::observeViewPosAndZoom)
                .def(
                    "setZoom",
                    &Viewport::setViewPosAndZoom,
                    py::arg("zoom"),
                    py::arg("focus"))
                .def_property(
                    "zoomRange",
                    &Viewport::getZoomRange,
                    &Viewport::setZoomRange,
                    py::return_value_policy::copy)
                .def_property(
                    "frameView",
                    &Viewport::hasFrameView,
                    &Viewport::setFrameView)
                .def_property_readonly(
                    "observeFrameView",
                    &Viewport::observeFrameView)
                .def_property_readonly(
                    "observeFramed",
                    &Viewport::observeFramed)
                .def("resetZoom", &Viewport::resetZoom)
                .def("zoomIn", &Viewport::zoomIn)
                .def("zoomOut", &Viewport::zoomOut)
                .def_property_readonly(
                    "FPS",
                    &Viewport::getFPS)
                .def_property_readonly(
                    "observeFPS",
                    &Viewport::observeFPS)
                .def_property_readonly(
                    "droppedFrames",
                    &Viewport::getDroppedFrames)
                .def_property_readonly(
                    "observeDroppedFrames",
                    &Viewport::observeDroppedFrames)
                .def("getColorSample", &Viewport::getColorSample)
                .def_property_readonly(
                    "observeSamplePos",
                    &Viewport::observeSamplePos)
                .def_property_readonly(
                    "observePick",
                    &Viewport::observePick)
                .def_property_readonly(
                    "observeColorSample",
                    &Viewport::observeColorSample)
                .def("pick", &Viewport::pick, py::arg("imagePos"))
                .def_property(
                    "inputEnabled",
                    &Viewport::isInputEnabled,
                    &Viewport::setInputEnabled)
                .def(
                    "setPanBinding",
                    &Viewport::setPanBinding,
                    py::arg("button"),
                    py::arg("modifier"))
                .def(
                    "setWipeBinding",
                    &Viewport::setWipeBinding,
                    py::arg("button"),
                    py::arg("modifier"))
                .def(
                    "setPickBinding",
                    &Viewport::setPickBinding,
                    py::arg("button"),
                    py::arg("modifier"))
                .def(
                    "setMouseWheelScale",
                    &Viewport::setMouseWheelScale);
        }
    }
}

