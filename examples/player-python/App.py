# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import opentimelineio as otio
import ftkPy as ftk
import tlRenderPy as tl

import DocumentModel
import MainWindow
import SettingsModel

import pathlib
import weakref

class App(ftk.App):
    """
    The application creates the models and main window.
    """
    def __init__(self, context, argv):
        
        # Command line arguments.
        self._cmdLineInput = ftk.CmdLineArgString("Input", "Input file", True)
        
        ftk.App.__init__(
            self,
            context,
            argv,
            "player-python",
            "Python player example",
            [ self._cmdLineInput ])

    def __del__(self):
        
        # Save time units settings.
        self._settingsModel.setString("/TimeUnits",
            tl.to_string(self._timeUnitsModel.timeUnits))

        # Save recent files settings.
        self._settingsModel.setStringList("/Files/Recent", self._recentFilesModel.recent)

    def getSettingsModel(self):
        """
        Get the settings model.
        """
        return self._settingsModel

    def getTimeUnitsModel(self):
        """
        Get the time units model.
        """
        return self._timeUnitsModel

    def getRecentFilesModel(self):
        """
        Get the recent files model.
        """
        return self._recentFilesModel

    def getDocumentModel(self):
        """
        Get the document model.
        """
        return self._documentModel

    def run(self):

        # Create the settings model.
        self._settingsModel = SettingsModel.Model(self.context)

        # Create the time units model.
        self._timeUnitsModel = tl.TimeUnitsModel(self.context)
        settings = self._settingsModel.getString("/TimeUnits")
        if settings[0]:
            tl.from_string(settings[1], self._timeUnitsModel.timeUnits)

        # Create the recent files model.
        self._recentFilesModel = ftk.RecentFilesModel(self.context)
        self._recentFilesModel.recent = self._settingsModel.getStringList("/Files/Recent")[1]

        # Create the document model.
        self._documentModel = DocumentModel.Model(self.context, self)
        
        # Initialize the file browser.
        fileBrowserSystem = self.context.getSystemByName("ftk::FileBrowserSystem")
        fileBrowserSystem.model.exts = tl.getExts(self.context)
        fileBrowserSystem.recentFilesModel = self._recentFilesModel

        # Create the main window.
        self._window = MainWindow.MainWindow(self.context, self)
        
        # Create an observer to update the recent files.
        selfWeak = weakref.ref(self)
        self._playerObserver = tl.PlayerObserver(
            self._documentModel.observePlayer(),
            lambda player: selfWeak()._playerUpdate(player))

        # Open command line inputs.
        if self._cmdLineInput.hasValue:
            self._documentModel.open(ftk.Path(self._cmdLineInput.value))

        super().run()

    def _playerUpdate(self, player):
        if player:
            self._recentFilesModel.addRecent(player.path.get())
