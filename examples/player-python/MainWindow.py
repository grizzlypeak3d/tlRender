# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import opentimelineio as otio
import feather_tk as ftk
import tlRenderPy as tl

import FileActions
import Menus
import PlaybackActions
import PlaybackBar
import SettingsWidget
import StatusBar
import ToolBars
import ViewActions
import WindowActions

import weakref

class MainWindow(ftk.MainWindow):
    """
    The main window creates the widgets and actions.
    """
    def __init__(self, context, app):
        ftk.MainWindow.__init__(self, context, app, ftk.Size2I(1280, 960))

        # Restore settings.
        settingsVisible = False
        splitter = 0.8
        splitter2 = 0.8
        self._settingsModel = app.getSettingsModel()
        settings = self._settingsModel.getBool("/MainWindow/SettingsVisible")
        if settings[0]:
            settingsVisible = settings[1]
        settings = self._settingsModel.getDouble("/MainWindow/Splitter")
        if settings[0]:
            splitter = settings[1]
        settings = self._settingsModel.getDouble("/MainWindow/Splitter2")
        if settings[0]:
            splitter2 = settings[1]
        self._settingsVisible = ftk.ObservableBool(settingsVisible)

        # Create the viewport.
        self._viewport = tl.ui.Viewport(context)
        
        # Create the timeline.
        self._timelineWidget = tl.ui.TimelineWidget(context, app.getTimeUnitsModel())

        # Create the actions.
        self._fileActions = FileActions.Actions(context, app, self)
        self._playbackActions = PlaybackActions.Actions(context, app)
        self._viewActions = ViewActions.Actions(context, app, self)
        self._windowActions = WindowActions.Actions(context, app, self)

        # Create the menu bar.
        self._menuBar = ftk.MenuBar(context)
        self._menuBar.addMenu("File", Menus.File(context, app, self._fileActions))
        self._menuBar.addMenu("Playback", Menus.Playback(context, app, self._playbackActions))
        self._menuBar.addMenu("View", Menus.View(context, app, self._viewActions))
        self._menuBar.addMenu("Window", Menus.Window(context, app, self._windowActions))
        self.menuBar = self._menuBar

        # Create the tool bars.
        self._fileToolBar = ToolBars.File(context, self._fileActions)
        self._viewToolBar = ToolBars.View(context, self._viewActions)
        self._windowToolBar = ToolBars.Window(context, self._windowActions)
        self._playbackBar = PlaybackBar.Widget(context, app, self._playbackActions)
        self._statusBar = StatusBar.Widget(context, app, self)

        # Create the settings widget.
        self._settingsWidget = SettingsWidget.Widget(context, app)
        self._settingsWidget.setVisible(settingsVisible)

        # Layout widgets.
        self._layout = ftk.VerticalLayout(context)
        self._layout.spacingRole = ftk.SizeRole._None
        self.widget = self._layout
        hLayout = ftk.HorizontalLayout(context, self._layout)
        hLayout.spacingRole = ftk.SizeRole.SpacingSmall
        self._fileToolBar.parent = hLayout
        ftk.Divider(context, ftk.Orientation.Horizontal, hLayout)
        self._viewToolBar.parent = hLayout
        ftk.Divider(context, ftk.Orientation.Horizontal, hLayout)
        self._windowToolBar.parent = hLayout
        ftk.Divider(context, ftk.Orientation.Vertical, self._layout)
        self._splitter = ftk.Splitter(context, ftk.Orientation.Vertical, self._layout)
        self._splitter.split = splitter
        self._splitter2 = ftk.Splitter(context, ftk.Orientation.Horizontal, self._splitter)
        self._splitter2.split = splitter2
        self._viewport.parent = self._splitter2
        self._settingsWidget.parent = self._splitter2
        vLayout = ftk.VerticalLayout(context, self._splitter)
        vLayout.spacingRole = ftk.SizeRole._None
        self._playbackBar.parent = vLayout
        ftk.Divider(context, ftk.Orientation.Vertical, vLayout)
        self._timelineWidget.parent = vLayout
        ftk.Divider(context, ftk.Orientation.Vertical, self._layout)
        self._statusBar.parent = self._layout

        # Create observers.
        selfWeak = weakref.ref(self)
        self.playerObserver = tl.PlayerObserver(
            app.getDocumentModel().observePlayer(),
            lambda player: selfWeak()._widgetUpdate(player))

    def __del__(self):
    
        # Save settings.
        self._settingsModel.setBool(
            "/MainWindow/SettingsVisible",
            self._settingsVisible.get())
        self._settingsModel.setDouble("/MainWindow/Splitter", self._splitter.split)
        self._settingsModel.setDouble("/MainWindow/Splitter2", self._splitter2.split)

    def getViewport(self):
        """
        Get the viewport.
        """
        return self._viewport

    def getSettingsVisible(self):
        return self._settingsVisible

    def setSettingsVisible(self, value):
        """
        Set whether the settings widget is visible.
        """
        if self._settingsVisible.setIfChanged(value):
            self._settingsWidget.setVisible(value)

    def dropEvent(self, event):
        event.accept = True
        if isinstance(event.data, ftk.DragDropTextData):
            if event.data.text:
                self.app.getDocumentModel().open(event.data.text[0])

    def _widgetUpdate(self, player):
        self._viewport.player = player
        self._timelineWidget.player = player
