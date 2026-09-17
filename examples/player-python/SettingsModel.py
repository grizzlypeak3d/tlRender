# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import opentimelineio as otio
import feather_tk as ftk
import tlrender as tl

import json
import weakref

class Model(ftk.Settings):
    """
    This model provides settings that are saved and restored.
    """
    def __init__(self, context):
        docPath = ftk.getUserPath(ftk.UserPath.Documents)
        settingsPath = ftk.getSettingsPath(
            ftk.Path(docPath, "tlRender").get(),
            "player-python.json")
        ftk.Settings.__init__(self, context, settingsPath)
        
        self._contextWeak = weakref.ref(context)

        # Restore file browser settings.
        fileBrowserSystem = context.getSystemByName("ftk::FileBrowserSystem")
        settings = self.getJSON("/FileBrowser")
        if settings[0]:
            ftk.from_json(settings[1], fileBrowserSystem.model.options)
        settings = self.getBool("/NativeFileDialog")
        if settings[0]:
            fileBrowserSystem.nativeFileDialog = settings[1]

        # Restore timeline player cache settings.
        cacheOptions = tl.PlayerCacheOptions()
        settings = self.getJSON("/Cache")
        if settings[0]:
            tl.from_json(settings[1], cacheOptions)
        self._cache = tl.ObservablePlayerCacheOptions(cacheOptions)

    def __del__(self):
    
        # Save file browse settings.
        fileBrowserSystem = self._contextWeak().getSystemByName("ftk::FileBrowserSystem")
        self.setJSON("/FileBrowser", ftk.to_json(fileBrowserSystem.model.options))
        self.setBool("/NativeFileDialog", fileBrowserSystem.nativeFileDialog)
        
        # Save timeline player cache settings.
        self.setJSON("/Cache", tl.to_json(self._cache.get()))

    def observeCache(self):
        """
        Observe the timeline player cache settings.
        """
        return self._cache

    def setCache(self, value):
        """
        Set the timeline player cache settings.
        """
        self._cache.setIfChanged(value)
