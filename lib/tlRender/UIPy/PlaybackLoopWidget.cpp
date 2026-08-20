// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/PlaybackLoopWidget.h>

#include <tlRender/UI/PlaybackLoopWidget.h>

#include <ftk/Core/Context.h>

#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void playbackLoopWidget(py::module_& m)
        {
            using namespace ui;

            py::class_<PlaybackLoopWidget, ftk::IContainer, std::shared_ptr<PlaybackLoopWidget> >(m, "PlaybackLoopWidget")
                .def(
                    py::init(&PlaybackLoopWidget::create),
                    py::arg("context"),
                    py::arg("parent") = nullptr)
                .def_property(
                    "loop",
                    &PlaybackLoopWidget::getLoop,
                    &PlaybackLoopWidget::setLoop)
                .def("setCallback", &PlaybackLoopWidget::setCallback);
        }
    }
}

