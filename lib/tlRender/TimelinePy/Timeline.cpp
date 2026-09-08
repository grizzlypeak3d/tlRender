// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/Context.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/set.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void timeline(nb::module_& m)
        {
            nb::class_<Timeline>(m, "Timeline")
                .def(nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const ftk::Path&,
                        const Options&>(&Timeline::create)),
                    nb::arg("context"),
                    nb::arg("path"),
                    nb::arg("options") = Options())
                .def(nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const ftk::Path&,
                        const ftk::Path&,
                        const Options&>(&Timeline::create)),
                    nb::arg("context"),
                    nb::arg("path"),
                    nb::arg("audioPath"),
                    nb::arg("options") = Options())
                .def(nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::string&,
                        const Options&>(&Timeline::create)),
                    nb::arg("context"),
                    nb::arg("fileName"),
                    nb::arg("options") = Options())
                .def(nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::string&,
                        const std::string&,
                        const Options&>(&Timeline::create)),
                    nb::arg("context"),
                    nb::arg("fileName"),
                    nb::arg("audioFileName"),
                    nb::arg("options") = Options())
                // From an opentimelineio Timeline object, by JSON
                // round-trip: tlRender's own OTIO parses the string, so
                // this works with any opentimelineio wheel (see
                // OTIOCasters.h for why the objects cannot be shared
                // directly). Last so the path and string overloads match
                // their own arguments first.
                .def(
                    nb::new_([](
                        const std::shared_ptr<ftk::Context>& context,
                        nb::handle otioTimeline,
                        const Options& options)
                    {
                        const std::string json = nb::cast<std::string>(
                            otioTimeline.attr("to_json_string")());
                        OTIO_NS::ErrorStatus errorStatus;
                        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otio(
                            dynamic_cast<OTIO_NS::Timeline*>(
                                OTIO_NS::Timeline::from_json_string(
                                    json, &errorStatus)));
                        if (!otio)
                        {
                            throw std::runtime_error(
                                "Cannot read the timeline: " +
                                errorStatus.details);
                        }
                        return Timeline::create(context, otio, options);
                    }),
                    nb::arg("context"),
                    nb::arg("otioTimeline"),
                    nb::arg("options") = Options())
                .def_prop_ro("context", &Timeline::getContext)
                // The reverse trip: hand Python an object made by its
                // own opentimelineio package.
                .def_prop_ro(
                    "otioTimeline",
                    [](const Timeline& self)
                    {
                        return nb::module_::import_("opentimelineio.adapters")
                            .attr("read_from_string")(
                                self.getOTIOTimeline().value->to_json_string(),
                                "otio_json");
                    })
                .def_prop_ro("path", &Timeline::getPath, nb::rv_policy::copy)
                .def_prop_ro("audioPath", &Timeline::getAudioPath, nb::rv_policy::copy)
                .def_prop_ro("options", &Timeline::getOptions, nb::rv_policy::copy)
                .def_prop_ro("timeRange", &Timeline::getTimeRange, nb::rv_policy::copy)
                .def_prop_ro("duration", &Timeline::getDuration)
                .def_prop_ro("ioInfo", &Timeline::getIOInfo, nb::rv_policy::copy)
                .def("getMediaTime", &Timeline::getMediaTime, nb::arg("time"))
                .def(
                    "getTimelineTime",
                    &Timeline::getTimelineTime,
                    nb::arg("time"),
                    nb::arg("mediaTime"))
                .def("getMediaFrame", &Timeline::getMediaFrame, nb::arg("time"))
                .def(
                    "getMediaFrameTime",
                    &Timeline::getMediaFrameTime,
                    nb::arg("time"),
                    nb::arg("frame"))
                .def("isMediaTimeContinuous", &Timeline::isMediaTimeContinuous);
        }
    }
}
