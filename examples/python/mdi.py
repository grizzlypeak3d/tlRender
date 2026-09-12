# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

# This example displays multiple timelines in a MDI widget.

import sys
import opentimelineio as otio
import ftkPy as ftk
import tlRenderPy as tl

import os

class MainWindow(ftk.MainWindow):
    def __init__(self, context):
        ftk.MainWindow.__init__(self, context, app, ftk.Size2I(1280, 960))

        self._viewports = []

        # Create a scroll widget.
        self._scrollWidget = ftk.ScrollWidget(context)
        self._scrollWidget.border = False
        self._scrollWidget.areaResizable = False
        self.widget = self._scrollWidget

        # Create a MDI canvas.
        self._mdiCanvas = ftk.MDICanvas(context)
        self._mdiCanvas.setSize(ftk.Size2I(8192, 8192))
        self._scrollWidget.widget = self._mdiCanvas

        # Create a MDI mini-map.
        self._miniMap = ftk.MDIMiniMap(context)
        self._scrollWidget.viewportWidget = self._miniMap

        # Setup callbacks.
        self._scrollWidget.setScrollInfoCallback(self._scrollInfoCallback)
        self._mdiCanvas.setChildGeometryCallback(self._childGeometryCallback)
        self._miniMap.setCallback(self._miniMapCallback)

    def dropEvent(self, event):
        event.accept = True
        if isinstance(event.data, ftk.DragDropTextData):
            for text in event.data.text:

                # \todo Add exception handling.
                timeline = tl.Timeline(self.context, text)
                player = tl.Player(self.context, timeline)

                viewport = tl.ui.Viewport(self.context)
                viewport.player = player
                self._viewports.append(viewport)

                playbackToolBar = tl.ui.PlaybackToolBar(self.context)
                playbackToolBar.player = player
                frameToolBar = tl.ui.FrameToolBar(self.context)
                frameToolBar.player = player

                timelineWidget = tl.ui.TimelineWidget(self.context)
                timelineDisplayOptions = tl.ui.DisplayOptions()
                timelineDisplayOptions.minimize = False
                timelineWidget.displayOptions = timelineDisplayOptions
                timelineWidget.vStretch = ftk.Stretch.Expanding
                timelineWidget.player = player
                
                splitter = ftk.Splitter(context, ftk.Orientation.Vertical)
                viewport.parent = splitter
                vLayout = ftk.VerticalLayout(context, splitter)
                vLayout.spacingRole = ftk.SizeRole._None
                hLayout = ftk.HorizontalLayout(context, vLayout)
                hLayout.spacingRole = ftk.SizeRole._None
                playbackToolBar.parent = hLayout
                frameToolBar.parent = hLayout
                timelineWidget.parent = vLayout
                self._mdiCanvas.addWidget(text, event.pos, splitter)

    def keyPressEvent(self, event):
        # The viewports have no keys of their own; the one under the
        # cursor gets them here.
        viewport = self._viewportAt(event.pos)
        if viewport and 0 == event.modifiers:
            if ftk.Key.Backspace == event.key:
                event.accept = True
                viewport.frameView = True
            elif ftk.Key._0 == event.key:
                event.accept = True
                viewport.resetZoom()
            elif ftk.Key.Equals == event.key:
                event.accept = True
                viewport.zoomIn()
            elif ftk.Key.Minus == event.key:
                event.accept = True
                viewport.zoomOut()
        if not event.accept:
            ftk.MainWindow.keyPressEvent(self, event)

    def _viewportAt(self, pos):
        for viewport in self._viewports:
            # A closed window's viewport is still in the list, but no
            # longer in this window.
            if viewport.window and ftk.contains(viewport.geometry, pos):
                return viewport
        return None

    def _scrollInfoCallback(self, info):
        self._miniMap.setScrollInfo(info)

    def _childGeometryCallback(self, geom):
        self._miniMap.setChildGeometry(geom)

    def _miniMapCallback(self, pos):
        self._scrollWidget.setScrollPos(pos)

# Create the application.
context = ftk.Context()
tl.ui.init(context)
app = ftk.App(context, sys.argv, "mdi", "Python MDI example.")
if app.hasCmdLineHelp:
    sys.exit(0)

# Create the main window.
window = MainWindow(context)

# Run the application.
app.run()

# Clean up.
window = None
app = None

