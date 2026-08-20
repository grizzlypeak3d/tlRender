# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

import ftkPy as ftk
import tlRenderPy as tl

import os
import unittest

@unittest.skipIf(os.environ.get("TLRENDER_TESTS_NO_GL"), "OpenGL is not available")
class UITest(unittest.TestCase):

    def setUp(self):
        self.context = ftk.Context()
        tl.ui.init(self.context)

    def test_timeline_widget(self):
        widget = tl.ui.TimelineWidget(self.context)
        self.assertIsNotNone(widget.timeUnitsModel)
        displayOptions = tl.ui.DisplayOptions()
        displayOptions.thumbnails = False
        widget.displayOptions = displayOptions
        self.assertEqual(displayOptions, widget.displayOptions)

    def test_viewport(self):
        viewport = tl.ui.Viewport(self.context)
        compareOptions = tl.CompareOptions()
        compareOptions.compare = tl.Compare.Wipe
        compareOptions.wipeRotation = 90.0
        viewport.compareOptions = compareOptions
        self.assertEqual(compareOptions, viewport.compareOptions)
        backgroundOptions = tl.BackgroundOptions()
        backgroundOptions.type = tl.Background.Checkers
        viewport.backgroundOptions = backgroundOptions
        self.assertEqual(backgroundOptions, viewport.backgroundOptions)

    def test_playback_loop_widget(self):
        widget = tl.ui.PlaybackLoopWidget(self.context)
        self.assertEqual(tl.Loop.Loop, widget.loop)
        widget.loop = tl.Loop.PingPong
        self.assertEqual(tl.Loop.PingPong, widget.loop)
        widget.loop = tl.Loop.Once
        self.assertEqual(tl.Loop.Once, widget.loop)

    def test_time_widgets(self):
        timeUnitsModel = tl.TimeUnitsModel(self.context)
        timeUnitsModel.timeUnits = tl.TimeUnits.Frames
        self.assertEqual(tl.TimeUnits.Frames, timeUnitsModel.timeUnits)
        timeEdit = tl.ui.TimeEdit(self.context, timeUnitsModel)
        self.assertIsNotNone(timeEdit)
        timeLabel = tl.ui.TimeLabel(self.context, timeUnitsModel)
        self.assertIsNotNone(timeLabel)
        timeUnitsWidget = tl.ui.TimeUnitsWidget(self.context, timeUnitsModel)
        self.assertIsNotNone(timeUnitsWidget)

    def test_timeline_ruler(self):
        itemData = tl.ui.ItemData()
        itemData.speed = 24.0
        itemData.dir = "/tmp"
        self.assertEqual(24.0, itemData.speed)
        self.assertEqual("/tmp", itemData.dir)
        ruler = tl.ui.TimelineRuler(self.context, itemData)
        self.assertIsNotNone(ruler)

if __name__ == '__main__':
    unittest.main()
