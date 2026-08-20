// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Bindings.h>

#include <tlRender/UIPy/ItemOptions.h>
#include <tlRender/UIPy/PlaybackLoopWidget.h>
#include <tlRender/UIPy/TimeEdit.h>
#include <tlRender/UIPy/TimeLabel.h>
#include <tlRender/UIPy/TimeUnitsWidget.h>
#include <tlRender/UIPy/TimelineRuler.h>
#include <tlRender/UIPy/TimelineWidget.h>
#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/Init.h>

#include <ftk/Core/Context.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void uiBind(py::module_& m)
        {
            auto mUI = m.def_submodule("ui", "User interface");
            
            mUI.def(
                "init",
                &ui::init,
                py::arg("context"),
                "Initialize the library.");

            itemOptions(mUI);

            frameToolBar(mUI);
            playbackToolBar(mUI);
            playbackLoopWidget(mUI);
            timeEdit(mUI);
            timeLabel(mUI);
            timeUnitsWidget(mUI);
            timelineWidget(mUI);
            timelineRuler(mUI);
            viewport(mUI);
        }
    }
}

