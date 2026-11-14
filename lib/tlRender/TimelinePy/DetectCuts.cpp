// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <tlRender/Timeline/DetectCuts.h>

#include <nanobind/operators.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void detectCuts(nb::module_& m)
        {
            nb::class_<DetectCutsOptions>(m, "DetectCutsOptions",
                "Options for finding the cuts in a timeline. The defaults "
                "come from two films' edits checked against what was found.")
                .def(nb::init<>())
                .def_rw("width", &DetectCutsOptions::width,
                    "The width frames are scored at.")
                .def_rw("threshold", &DetectCutsOptions::threshold,
                    "A cut is a frame scoring at least this, from 0 to 1.")
                .def_rw("blackThreshold", &DetectCutsOptions::blackThreshold,
                    "The threshold after a black frame: a cut out of black "
                    "scores lower than other cuts.")
                .def_rw("blackLevel", &DetectCutsOptions::blackLevel,
                    "A frame is black when its average luma is at most this.")
                .def_rw("relativeMin", &DetectCutsOptions::relativeMin,
                    "A frame under the threshold is still a cut when it scores "
                    "at least this, and relativeRatio times the frames around "
                    "it.")
                .def_rw("relativeRatio", &DetectCutsOptions::relativeRatio,
                    "How far above the frames around it; zero turns it off.")
                .def_rw("relativeWindow", &DetectCutsOptions::relativeWindow,
                    "How many frames on either side are around it.")
                .def_rw("minLength", &DetectCutsOptions::minLength,
                    "Cuts closer together than this many frames are one, the "
                    "first of them.")
                .def(nb::self == nb::self);

            nb::class_<CutScore>(m, "CutScore",
                "How different a frame is from the one before, from 0 to 1, "
                "as FFmpeg's scene filter scores it, and its average luma.")
                .def(nb::init<>())
                .def_rw("score", &CutScore::score)
                .def_rw("luma", &CutScore::luma)
                .def(nb::self == nb::self);

            m.def(
                "findCuts",
                &findCuts,
                nb::arg("scores"),
                nb::arg("options") = DetectCutsOptions(),
                "The indexes of the frames that are cut to, from a "
                "list of scores; for trying other rules on scores already "
                "made.");

            m.def(
                "scoreFrames",
                [](const std::shared_ptr<Timeline>& timeline,
                    const OTIO_NS::TimeRange& range,
                    const DetectCutsOptions& options)
                {
                    nb::gil_scoped_release release;
                    return scoreFrames(timeline, range, options);
                },
                nb::arg("timeline"),
                nb::arg("range"),
                nb::arg("options") = DetectCutsOptions(),
                "Score each frame of a timeline over a range against the one "
                "before it. Reads every frame; give it a timeline of its own.");

            m.def(
                "detectCuts",
                [](const std::shared_ptr<Timeline>& timeline,
                    const OTIO_NS::TimeRange& range,
                    const DetectCutsOptions& options)
                {
                    nb::gil_scoped_release release;
                    return detectCuts(timeline, range, options);
                },
                nb::arg("timeline"),
                nb::arg("range"),
                nb::arg("options") = DetectCutsOptions(),
                "The times of the frames that are cut to, over a "
                "range: the cuts in an edit flattened to one movie or image "
                "sequence. Hard cuts only. Reads every frame; give it a "
                "timeline of its own.");
        }
    }
}
