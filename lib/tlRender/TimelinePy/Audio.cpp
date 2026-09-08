// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Audio.h>

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
        void timelineAudio(nb::module_& m)
        {
            nb::class_<AudioLayer>(m, "AudioLayer")
                .def(nb::init())
                .def_rw("audio", &AudioLayer::audio)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<AudioFrame>(m, "AudioFrame")
                .def(nb::init())
                .def_rw("seconds", &AudioFrame::seconds)
                .def_rw("layers", &AudioFrame::layers)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def(
                "isTimeEqual",
                [](const AudioFrame& a, const AudioFrame& b)
                {
                    return isTimeEqual(a, b);
                });
        }
    }
}
