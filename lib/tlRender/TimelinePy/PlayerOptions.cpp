// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/PlayerOptions.h>

#include <nanobind/stl/chrono.h>
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
        void playerOptions(nb::module_& m)
        {
            nb::class_<PlayerCacheOptions>(m, "PlayerCacheOptions")
                .def(nb::init())
                .def_rw("videoGB", &PlayerCacheOptions::videoGB)
                .def_rw("audioGB", &PlayerCacheOptions::audioGB)
                .def_rw("readBehind", &PlayerCacheOptions::readBehind)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::class_<PlayerOptions>(m, "PlayerOptions")
                .def(nb::init())
                .def_rw("audioDevice", &PlayerOptions::audioDevice)
                .def_rw("cache", &PlayerOptions::cache)
                .def_rw("videoRequestMax", &PlayerOptions::videoRequestMax)
                .def_rw("audioRequestMax", &PlayerOptions::audioRequestMax)
                .def_rw("audioBufferFrameCount", &PlayerOptions::audioBufferFrameCount)
                .def_rw("muteTimeout", &PlayerOptions::muteTimeout)
                .def_rw("sleepTimeout", &PlayerOptions::sleepTimeout)
                .def_rw("currentTime", &PlayerOptions::currentTime)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            m.def("to_json",
                [](const PlayerCacheOptions& value)
                {
                    nlohmann::json json;
                    to_json(json, value);
                    return json.dump();
                });

            m.def("from_json",
                [](const std::string& value, PlayerCacheOptions& out)
                {
                    from_json(nlohmann::json().parse(value), out);
                });
        }
    }
}

