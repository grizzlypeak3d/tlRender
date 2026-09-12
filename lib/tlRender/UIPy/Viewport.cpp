// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/Viewport.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <nanobind/stl/function.h>
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
        void viewport(nb::module_& m)
        {
            using namespace ui;

            ftk::python::observable<std::pair<ftk::V2I, double> >(m, "ViewPosAndZoom");
            ftk::python::observable<std::optional<ftk::V2I> >(m, "OptionalV2I");
            ftk::python::observable<std::optional<ftk::Color4F> >(m, "OptionalColor4F");
            
            nb::class_<Viewport, ftk::IWidget>(m, "Viewport")
                .def(
                    nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<ftk::IWidget>&>(&Viewport::create)),
                    nb::arg("context"),
                    nb::arg("parent") = nullptr)
                .def_prop_rw("compareOptions",
                    &Viewport::getCompareOptions,
                    &Viewport::setCompareOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeCompareOptions",
                    &Viewport::observeCompareOptions)
                .def_prop_rw(
                    "ocioOptions",
                    &Viewport::getOCIOOptions,
                    &Viewport::setOCIOOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeOCIOOptions",
                    &Viewport::observeOCIOOptions)
                .def_prop_rw(
                    "LUTOptions",
                    &Viewport::getLUTOptions,
                    &Viewport::setLUTOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeLUTOptions",
                    &Viewport::observeLUTOptions)
                .def_prop_rw(
                    "imageOptions",
                    &Viewport::getImageOptions,
                    &Viewport::setImageOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeImageOptions",
                    &Viewport::observeImageOptions)
                .def_prop_rw(
                    "displayOptions",
                    &Viewport::getDisplayOptions,
                    &Viewport::setDisplayOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeDisplayOptions",
                    &Viewport::observeDisplayOptions)
                .def_prop_rw(
                    "backgroundOptions",
                    &Viewport::getBackgroundOptions,
                    &Viewport::setBackgroundOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeBackgroundOptions",
                    &Viewport::observeBackgroundOptions)
                .def_prop_rw(
                    "foregroundOptions",
                    &Viewport::getForegroundOptions,
                    &Viewport::setForegroundOptions,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeForegroundOptions",
                    &Viewport::observeForegroundOptions)
                .def_prop_rw(
                    "colorBuffer",
                    &Viewport::getColorBuffer,
                    &Viewport::setColorBuffer)
                .def_prop_ro(
                    "observeColorBuffer",
                    &Viewport::observeColorBuffer)
                .def_prop_rw(
                    "player",
                    &Viewport::getPlayer,
                    &Viewport::setPlayer,
                    // "No player" is a null player.
                    nb::for_setter(nb::arg("value").none()))
                .def_prop_ro(
                    "viewPos",
                    &Viewport::getViewPos,
                    nb::rv_policy::copy)
                .def_prop_ro(
                    "observeViewPos",
                    &Viewport::observeViewPos)
                .def_prop_ro(
                    "zoom",
                    &Viewport::getZoom)
                .def_prop_ro(
                    "observeZoom",
                    &Viewport::observeZoom)
                .def_prop_ro(
                    "viewPosAndZoom",
                    &Viewport::getViewPosAndZoom)
                .def(
                    "setViewPosAndZoom",
                    &Viewport::setViewPosAndZoom,
                    nb::arg("pos"),
                    nb::arg("zoom"))
                .def_prop_ro(
                    "observeViewPosAndZoom",
                    &Viewport::observeViewPosAndZoom)
                .def(
                    "setZoom",
                    &Viewport::setZoom,
                    nb::arg("zoom"),
                    nb::arg("focus") = ftk::V2I())
                .def("center", &Viewport::center)
                .def("resetZoom", &Viewport::resetZoom)
                .def("zoomIn", &Viewport::zoomIn)
                .def("zoomOut", &Viewport::zoomOut)
                .def_prop_rw(
                    "zoomRange",
                    &Viewport::getZoomRange,
                    &Viewport::setZoomRange,
                    nb::rv_policy::copy)
                .def_prop_rw(
                    "frameView",
                    &Viewport::hasFrameView,
                    &Viewport::setFrameView)
                .def_prop_ro(
                    "observeFrameView",
                    &Viewport::observeFrameView)
                .def_prop_ro(
                    "observeFramed",
                    &Viewport::observeFramed)
                .def("resetZoom", &Viewport::resetZoom)
                .def("zoomIn", &Viewport::zoomIn)
                .def("zoomOut", &Viewport::zoomOut)
                .def("center", &Viewport::center)
                .def_prop_ro(
                    "FPS",
                    &Viewport::getFPS)
                .def_prop_ro(
                    "observeFPS",
                    &Viewport::observeFPS)
                .def_prop_ro(
                    "droppedFrames",
                    &Viewport::getDroppedFrames)
                .def_prop_ro(
                    "observeDroppedFrames",
                    &Viewport::observeDroppedFrames)
                .def("getColorSample", &Viewport::getColorSample)
                .def_prop_ro(
                    "observeSamplePos",
                    &Viewport::observeSamplePos)
                .def_prop_ro(
                    "observePick",
                    &Viewport::observePick)
                .def_prop_ro(
                    "observeColorSample",
                    &Viewport::observeColorSample)
                .def("pick", &Viewport::pick, nb::arg("imagePos"))
                .def_prop_rw(
                    "inputEnabled",
                    &Viewport::isInputEnabled,
                    &Viewport::setInputEnabled)
                .def(
                    "setPanBinding",
                    &Viewport::setPanBinding,
                    nb::arg("button"),
                    nb::arg("modifier"))
                .def(
                    "setWipeBinding",
                    &Viewport::setWipeBinding,
                    nb::arg("button"),
                    nb::arg("modifier"))
                .def(
                    "setPickBinding",
                    &Viewport::setPickBinding,
                    nb::arg("button"),
                    nb::arg("modifier"))
                .def(
                    "setMouseWheelScale",
                    &Viewport::setMouseWheelScale);
        }
    }
}

