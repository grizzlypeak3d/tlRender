// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/PlaybackLoopWidget.h>

#include <tlRender/UI/PlaybackLoopWidget.h>

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
        void playbackLoopWidget(nb::module_& m)
        {
            using namespace ui;

            nb::class_<PlaybackLoopWidget, ftk::IContainer>(m, "PlaybackLoopWidget")
                .def(
                    nb::new_(&PlaybackLoopWidget::create),
                    nb::arg("context"),
                    nb::arg("parent") = nullptr)
                .def_prop_rw(
                    "loop",
                    &PlaybackLoopWidget::getLoop,
                    &PlaybackLoopWidget::setLoop)
                .def("setCallback", &PlaybackLoopWidget::setCallback);
        }
    }
}

