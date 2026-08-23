// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <pybind11/pybind11.h>

namespace tl
{
    namespace python
    {
        void timelineAudio(pybind11::module_&);
        void audioSystem(pybind11::module_&);
        void backgroundOptions(pybind11::module_&);
        void colorOptions(pybind11::module_&);
        void compareOptions(pybind11::module_&);
        void displayOptions(pybind11::module_&);
        void foregroundOptions(pybind11::module_&);
        void iRender(pybind11::module_&);
        void player(pybind11::module_&);
        void playerOptions(pybind11::module_&);
        void timeline(pybind11::module_&);
        void timelineOptions(pybind11::module_&);
        void timelineSystem(pybind11::module_&);
        void timeUnits(pybind11::module_&);
        void transition(pybind11::module_&);
        void util(pybind11::module_&);
        void video(pybind11::module_&);

        void timelineBind(pybind11::module_&);
    }
}
