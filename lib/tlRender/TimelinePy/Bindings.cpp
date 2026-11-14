// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <tlRender/Timeline/Init.h>

#include <ftk/Core/Context.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void timelineBind(nb::module_& m)
        {
            m.def(
                "init",
                &init,
                nb::arg("context"),
                "Initialize the library: the I/O, audio and render systems, "
                "added to the context. Once, before making timelines or "
                "players. Without a display or sound card -- a container, a "
                "render farm -- set SDL_AUDIO_DRIVER=dummy in the environment "
                "first: audio then goes nowhere, and the system's audio "
                "libraries are not asked for devices, which is what prints "
                "their errors when there are none.");

            timelineAudio(m);
            audioSystem(m);
            backgroundOptions(m);
            colorOptions(m);
            compareOptions(m);
            // After colorOptions(): DisplayOptions carries the OCIO and LUT
            // options registered there.
            displayOptions(m);
            foregroundOptions(m);
            timeUnits(m);
            timelineOptions(m);
            timelineSystem(m);
            timeline(m);
            detectCuts(m);
            playerOptions(m);
            player(m);
            transition(m);
            util(m);
            video(m);
            iRender(m);
        }
    }
}

