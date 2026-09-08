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
                "Initialize the library.");

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
            playerOptions(m);
            player(m);
            transition(m);
            util(m);
            video(m);
            iRender(m);
        }
    }
}

