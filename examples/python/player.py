# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

# This example demonstrates a simple player application.
#
# The timeline can be specified on the command line like:
# * simple.py timeline.otio
# * simple.py timeline.otioz
# * simple.py render.mov
# * simple.py render.#.exr
#
# The timeline can be zoomed with the mouse wheel or "-" and "=" keys.
#
# The timeline can be panned with control + mouse or the scroll bars.

import sys
import opentimelineio as otio
import ftkPy as ftk
import tlRenderPy as tl

import os

# Create the application.
context = ftk.Context()
tl.ui.init(context)
cmdLineInput = ftk.CmdLineArgString("Input", "Input file")
app = ftk.App(context, sys.argv, "timeline", "Python timeline example.", [ cmdLineInput ])
if app.hasCmdLineHelp:
    sys.exit(0)

# Create the timeline and timeline player.
#
# \todo Add exception handling.
timeline = tl.Timeline(context, ftk.Path(cmdLineInput.value))
player = tl.Player(context, timeline)

# Create the viewport.
viewport = tl.ui.Viewport(context)
viewport.vStretch = ftk.Stretch.Expanding
viewport.player = player

# Create tool bars.
playbackToolBar = tl.ui.PlaybackToolBar(context)
playbackToolBar.player = player
frameToolBar = tl.ui.FrameToolBar(context)
frameToolBar.player = player

# Create the timeline widget.
timelineWidget = tl.ui.TimelineWidget(context)
timelineWidget.vStretch = ftk.Stretch.Expanding
timelineDisplayOptions = tl.ui.DisplayOptions()
timelineDisplayOptions.minimize = False
timelineWidget.displayOptions = timelineDisplayOptions
timelineWidget.player = player

# Create the layout and main window.
window = ftk.MainWindow(context, app)
splitter = ftk.Splitter(context, ftk.Orientation.Vertical)
splitter.split = 0.7
viewport.parent = splitter

vLayout = ftk.VerticalLayout(context, splitter)
vLayout.spacingRole = ftk.SizeRole._None

hLayout = ftk.HorizontalLayout(context, vLayout)
hLayout.spacingRole = ftk.SizeRole._None
playbackToolBar.parent = hLayout
ftk.Divider(context, ftk.Orientation.Horizontal, hLayout)
frameToolBar.parent = hLayout
ftk.Divider(context, ftk.Orientation.Vertical, vLayout)

timelineWidget.parent = vLayout
window.widget = splitter

# The viewport has no keys of its own; give it them as the shortcuts of a
# menu.
viewMenu = window.menuBar.addMenu("View")
viewMenu.addAction(ftk.Action(
    "Frame",
    ftk.KeyShortcut(ftk.Key.Backspace),
    lambda: setattr(viewport, "frameView", True)))
viewMenu.addAction(ftk.Action(
    "Zoom 1:1",
    ftk.KeyShortcut(ftk.Key._0),
    lambda: viewport.resetZoom()))
viewMenu.addAction(ftk.Action(
    "Zoom In",
    ftk.KeyShortcut(ftk.Key.Equals),
    lambda: viewport.zoomIn()))
viewMenu.addAction(ftk.Action(
    "Zoom Out",
    ftk.KeyShortcut(ftk.Key.Minus),
    lambda: viewport.zoomOut()))

# Run the application.
app.run()

# Clean up.
window = None
app = None
