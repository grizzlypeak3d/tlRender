# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

# This example combines a file browser with a timeline viewport and widget.

import sys
import opentimelineio as otio
import ftkPy as ftk
import tlRenderPy as tl

import os
import pathlib
import weakref

class MainWindow(ftk.MainWindow):
    def __init__(self, context, app):
        ftk.MainWindow.__init__(self, context, app, ftk.Size2I(1280, 960))

        self._player = None
        
        # Get the valid file extensions.
        self._exts = tl.getExts(context)
        
        # Create the file browser path widget.
        self._fileBrowserPathWidget = ftk.FileBrowserPath(context)
        path = os.getcwd()
        self._fileBrowserPathWidget.path = path

        # Create the file browser view.
        self._fileBrowserModel = ftk.FileBrowserModel(context)
        fileBrowserOptions = ftk.FileBrowserOptions()
        fileBrowserOptions.dirList.seqExts = tl.getExts(context, tl.FileType.Seq)
        self._fileBrowserModel.options = fileBrowserOptions
        self._fileBrowserModel.path = path
        self._fileBrowserView = ftk.FileBrowserView(
            context,
            ftk.FileBrowserMode.File,
            self._fileBrowserModel)

        # Create the viewport.
        self._viewport = tl.ui.Viewport(context)
        
        # Create the tool bars.
        self._playbackToolBar = tl.ui.PlaybackToolBar(context)
        self._frameToolBar = tl.ui.FrameToolBar(context)

        # Create the timeline widget.
        self._timelineWidget = tl.ui.TimelineWidget(context)
        self._timelineWidget.backgroundColor = ftk.ColorRole.Red
        self._timelineWidget.vStretch = ftk.Stretch.Expanding
        timelineDisplayOptions = tl.ui.DisplayOptions()
        timelineDisplayOptions.minimize = False
        self._timelineWidget.displayOptions = timelineDisplayOptions

        # Layout the widgets.
        self._layout = ftk.VerticalLayout(context)
        self._layout.spacingRole = ftk.SizeRole._None
        self.widget = self._layout

        layout = ftk.VerticalLayout(context, self._layout)
        layout.marginRole = ftk.SizeRole.MarginInside
        self._fileBrowserPathWidget.parent = layout
        ftk.Divider(context, ftk.Orientation.Vertical, self._layout)

        self._splitter = ftk.Splitter(context, ftk.Orientation.Horizontal, self._layout)
        self._splitter.split = 0.3
        scrollWidget = ftk.ScrollWidget(context, ftk.ScrollType.Both, self._splitter)
        scrollWidget.vStretch = ftk.Stretch.Expanding
        scrollWidget.border = False
        scrollWidget.widget = self._fileBrowserView

        self._splitter2 = ftk.Splitter(context, ftk.Orientation.Vertical, self._splitter)
        self._splitter2.split = 0.6
        self._viewport.parent = self._splitter2
        layout = ftk.VerticalLayout(context, self._splitter2)
        layout.spacingRole = ftk.SizeRole._None
        hLayout = ftk.HorizontalLayout(context, layout)
        hLayout.spacingRole = ftk.SizeRole.SpacingTool
        self._playbackToolBar.parent = hLayout
        ftk.Divider(context, ftk.Orientation.Horizontal, hLayout)
        self._frameToolBar.parent = hLayout
        ftk.Divider(context, ftk.Orientation.Vertical, layout)
        self._timelineWidget.parent = layout

        # Setup callbacks.
        selfWeak = weakref.ref(self)
        self._fileBrowserPathWidget.setCallback(
            lambda path: selfWeak()._pathUpdate(path))

        self._fileBrowserView.setSelectCallback(
            lambda path: selfWeak()._selectCallback(path))

        self._pathObserver = ftk.FileSystemPathObserver(
            self._fileBrowserModel.observePath,
            lambda path: selfWeak()._pathUpdate(path))

    def _selectCallback(self, path):
        
        self._player = None
        self._playbackToolBar.player = None
        self._frameToolBar.player = None
        self._viewport.player = None
        self._timelineWidget.player = None
        self._playbackObserver = None

        # Check for a valid file extension.
        ext = os.path.splitext(path.get())[1]
        if ext and ext.lower() in self._exts:
        
            # Create the timeline and player.
            #
            # \todo Add exception handling.
            timeline = tl.Timeline(self.context, path)
            self._player = tl.Player(self.context, timeline)
            self._playbackToolBar.player = self._player
            self._frameToolBar.player = self._player
            self._viewport.player = self._player
            self._timelineWidget.player = self._player

    def _pathUpdate(self, path):
        self._fileBrowserModel.path = path
        self._fileBrowserPathWidget.path = path

# Create the application.
context = ftk.Context()
tl.ui.init(context)
app = ftk.App(context, sys.argv, "browser-python", "Python browser example.")
if app.exitValue != 0:
    sys.exit(app.exitValue)

# Create the main window.
window = MainWindow(context, app)

app.run()

# Clean up.
window = None
app = None
