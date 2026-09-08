// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UIPy/Bindings.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <tlRender/UIPy/ItemOptions.h>
#include <tlRender/UIPy/PlaybackLoopWidget.h>
#include <tlRender/UIPy/ThumbnailSystem.h>
#include <tlRender/UIPy/TimeEdit.h>
#include <tlRender/UIPy/TimeLabel.h>
#include <tlRender/UIPy/TimeUnitsWidget.h>
#include <tlRender/UIPy/TimelineRuler.h>
#include <tlRender/UIPy/TimelineWidget.h>
#include <tlRender/UIPy/Viewport.h>

#include <tlRender/UI/Init.h>

#include <ftk/Core/Context.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void uiBind(nb::module_& m)
        {
            auto mUI = m.def_submodule("ui", "User interface");
            
            mUI.def(
                "init",
                &ui::init,
                nb::arg("context"),
                "Initialize the library.");

            itemOptions(mUI);

            frameToolBar(mUI);
            playbackToolBar(mUI);
            playbackLoopWidget(mUI);
            thumbnailSystem(mUI);
            timeEdit(mUI);
            timeLabel(mUI);
            timeUnitsWidget(mUI);
            timelineWidget(mUI);
            timelineRuler(mUI);
            viewport(mUI);
        }
    }
}

