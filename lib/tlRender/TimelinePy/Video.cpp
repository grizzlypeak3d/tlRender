// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Video.h>

#include <ftk/Core/Context.h>

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
        void video(nb::module_& m)
        {
            nb::class_<VideoLayer>(m, "VideoLayer")
                .def(nb::init())
                .def_rw("image", &VideoLayer::image)
                .def_rw("imageOptions", &VideoLayer::imageOptions)
                .def_rw("imageB", &VideoLayer::imageB)
                .def_rw("imageOptionsB", &VideoLayer::imageOptionsB)
                .def_rw("transition", &VideoLayer::transition)
                .def_rw("transitionValue", &VideoLayer::transitionValue)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<VideoFrame>(m, "VideoFrame")
                .def(nb::init())
                .def_rw("size", &VideoFrame::size)
                .def_rw("time", &VideoFrame::time)
                .def_rw("layers", &VideoFrame::layers)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def(
                "isTimeEqual",
                [](const VideoFrame& a, const VideoFrame& b)
                {
                    return isTimeEqual(a, b);
                });
        }
    }
}
