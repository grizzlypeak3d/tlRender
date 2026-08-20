// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        TL_API void timelineAudio(pybind11::module_&);
        TL_API void audioSystem(pybind11::module_&);
        TL_API void backgroundOptions(pybind11::module_&);
        TL_API void colorOptions(pybind11::module_&);
        TL_API void compareOptions(pybind11::module_&);
        TL_API void displayOptions(pybind11::module_&);
        TL_API void foregroundOptions(pybind11::module_&);
        TL_API void iRender(pybind11::module_&);
        TL_API void player(pybind11::module_&);
        TL_API void playerOptions(pybind11::module_&);
        TL_API void timeline(pybind11::module_&);
        TL_API void timelineOptions(pybind11::module_&);
        TL_API void timelineSystem(pybind11::module_&);
        TL_API void timeUnits(pybind11::module_&);
        TL_API void transition(pybind11::module_&);
        TL_API void util(pybind11::module_&);
        TL_API void video(pybind11::module_&);

        TL_API void timelineBind(pybind11::module_&);
    }
}
