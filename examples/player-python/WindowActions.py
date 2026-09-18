# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import opentimelineio as otio
import feather_tk as ftk
import tlrender as tl

import weakref

class Actions:
    """
    This class provides window actions.
    """
    def __init__(self, context, app, mainWindow):

        self._mainWindowWeak = weakref.ref(mainWindow)
        self.actions = {}
        self.actions["FullScreen"] = ftk.Action(
            "Full Screen",
            "WindowFullScreen",
            ftk.KeyShortcut(ftk.Key.U, ftk.commandKeyModifier),
            checkedCallback = self._fullScreenCallback)
        self.actions["FullScreen"].tooltip = "Toggle the window full screen."

        self.actions["Settings"] = ftk.Action(
            "Settings",
            "Settings",
            checkedCallback=self._settingsCallback)
        self.actions["Settings"].tooltip = "Toggle the settings."

        selfWeak = weakref.ref(self)
        self._settingsVisibleObserver = ftk.BoolObserver(
            mainWindow.getSettingsVisible(),
            lambda value: selfWeak()._settingsUpdate(value))

    def _fullScreenCallback(self, value):
        if self._mainWindowWeak:
            self._mainWindowWeak().fullScreen = value

    def _settingsUpdate(self, value):
        self.actions["Settings"].checked = value

    def _settingsCallback(self, value):
        if self._mainWindowWeak:
            self._mainWindowWeak().setSettingsVisible(value)
