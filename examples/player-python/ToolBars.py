# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import opentimelineio as otio
import feather_tk as ftk
import tlrender as tl

class File(ftk.ToolBar):
    """
    File tool bar.
    """
    def __init__(self, context, actions, parent = None):
        ftk.ToolBar.__init__(self, context, ftk.Orientation.Horizontal, parent)

        self.addAction(actions.actions["Open"])
        self.addAction(actions.actions["Close"])

class View(ftk.ToolBar):
    """
    View tool bar.
    """
    def __init__(self, context, actions, parent = None):
        ftk.ToolBar.__init__(self, context, ftk.Orientation.Horizontal, parent)

        self.addAction(actions.actions["Frame"])
        self.addAction(actions.actions["ZoomReset"])
        self.addAction(actions.actions["ZoomIn"])
        self.addAction(actions.actions["ZoomOut"])

class Window(ftk.ToolBar):
    """
    Window tool bar.
    """
    def __init__(self, context, actions, parent = None):
        ftk.ToolBar.__init__(self, context, ftk.Orientation.Horizontal, parent)

        self.addAction(actions.actions["FullScreen"])
        self.addAction(actions.actions["Settings"])
