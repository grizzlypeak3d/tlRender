// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/TimelineOptions.h>

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
        void timelineOptions(nb::module_& m)
        {
            FTK_ENUM_PY(m, ImageSeqAudio);
            FTK_ENUM_BIND(m, ImageSeqAudio);

            nb::class_<Options>(m, "Options")
                .def(nb::init())
                .def_rw("imageSeqAudio", &Options::imageSeqAudio)
                .def_rw("imageSeqAudioExts", &Options::imageSeqAudioExts)
                .def_rw("imageSeqAudioFileName", &Options::imageSeqAudioFileName)
                .def_rw("compat", &Options::compat)
                .def_rw("threaded", &Options::threaded)
                .def_rw("readThreadCount", &Options::readThreadCount)
                .def_rw("audioRequestMax", &Options::audioRequestMax)
                .def_rw("ioOptions", &Options::ioOptions)
                .def_rw("pathOptions", &Options::pathOptions)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);
        }
    }
}
