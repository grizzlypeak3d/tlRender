// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once


#include <nanobind/nanobind.h>

namespace tl
{
    namespace python
    {
        void timelineAudio(nanobind::module_&);
        void audioSystem(nanobind::module_&);
        void backgroundOptions(nanobind::module_&);
        void colorOptions(nanobind::module_&);
        void compareOptions(nanobind::module_&);
        void displayOptions(nanobind::module_&);
        void foregroundOptions(nanobind::module_&);
        void iRender(nanobind::module_&);
        void player(nanobind::module_&);
        void playerOptions(nanobind::module_&);
        void timeline(nanobind::module_&);
        void timelineOptions(nanobind::module_&);
        void timelineSystem(nanobind::module_&);
        void timeUnits(nanobind::module_&);
        void transition(nanobind::module_&);
        void util(nanobind::module_&);
        void video(nanobind::module_&);

        void timelineBind(nanobind::module_&);
    }
}
