# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

import ftkPy as ftk
import tlRenderPy as tl

import opentimelineio as otio

import os
import tempfile
import unittest

class OCIOOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.OCIOOptions()
        self.assertFalse(options.enabled)
        options.enabled = True
        options.config = tl.OCIOConfig.File
        options.fileName = "config.ocio"
        options.input = "input"
        options.display = "display"
        options.view = "view"
        options.look = "look"
        self.assertTrue(options.enabled)
        self.assertEqual(tl.OCIOConfig.File, options.config)
        self.assertEqual("config.ocio", options.fileName)
        self.assertEqual("input", options.input)
        self.assertEqual("display", options.display)
        self.assertEqual("view", options.view)
        self.assertEqual("look", options.look)

    def test_operators(self):
        a = tl.OCIOOptions()
        b = tl.OCIOOptions()
        self.assertEqual(a, b)
        b.enabled = True
        self.assertNotEqual(a, b)

class LUTOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.LUTOptions()
        self.assertFalse(options.enabled)
        self.assertEqual(tl.LUTOrder.PostConfig, options.order)
        options.enabled = True
        options.fileName = "test.cube"
        options.order = tl.LUTOrder.PreConfig
        self.assertTrue(options.enabled)
        self.assertEqual("test.cube", options.fileName)
        self.assertEqual(tl.LUTOrder.PreConfig, options.order)

    def test_operators(self):
        a = tl.LUTOptions()
        b = tl.LUTOptions()
        self.assertEqual(a, b)
        b.fileName = "test.cube"
        self.assertNotEqual(a, b)

class DisplayOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.DisplayOptions()
        color = tl.Color()
        color.enabled = True
        color.brightness = ftk.V3F(2, 2, 2)
        options.color = color
        self.assertEqual(color, options.color)
        levels = tl.Levels()
        levels.enabled = True
        levels.gamma = 2.2
        options.levels = levels
        self.assertEqual(levels, options.levels)

    def test_operators(self):
        a = tl.DisplayOptions()
        b = tl.DisplayOptions()
        self.assertEqual(a, b)
        color = tl.Color()
        color.enabled = True
        b.color = color
        self.assertNotEqual(a, b)

class BackgroundOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.BackgroundOptions()
        self.assertEqual(tl.Background.Solid, options.type)
        options.type = tl.Background.Checkers
        self.assertEqual(tl.Background.Checkers, options.type)
        options.solidColor = ftk.Color4F(1, 0, 0, 1)
        self.assertEqual(ftk.Color4F(1, 0, 0, 1), options.solidColor)
        outline = tl.Outline()
        outline.enabled = True
        outline.width = 4
        options.outline = outline
        self.assertEqual(outline, options.outline)

    def test_operators(self):
        a = tl.BackgroundOptions()
        b = tl.BackgroundOptions()
        self.assertEqual(a, b)
        b.type = tl.Background.Gradient
        self.assertNotEqual(a, b)

class ForegroundOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.ForegroundOptions()
        grid = tl.Grid()
        grid.enabled = True
        grid.cellSize = 50
        options.grid = grid
        self.assertEqual(grid, options.grid)

    def test_operators(self):
        a = tl.ForegroundOptions()
        b = tl.ForegroundOptions()
        self.assertEqual(a, b)
        grid = tl.Grid()
        grid.enabled = True
        b.grid = grid
        self.assertNotEqual(a, b)

class CompareOptionsTest(unittest.TestCase):

    def test_members(self):
        options = tl.CompareOptions()
        self.assertEqual(tl.Compare._None, options.compare)
        options.compare = tl.Compare.Wipe
        options.wipeCenter = ftk.V2F(.25, .25)
        options.wipeRotation = 90.0
        options.overlay = .5
        self.assertEqual(tl.Compare.Wipe, options.compare)
        self.assertEqual(ftk.V2F(.25, .25), options.wipeCenter)
        self.assertEqual(90.0, options.wipeRotation)
        self.assertEqual(.5, options.overlay)

    def test_operators(self):
        a = tl.CompareOptions()
        b = tl.CompareOptions()
        self.assertEqual(a, b)
        b.compare = tl.Compare.Difference
        self.assertNotEqual(a, b)

class ObserverTest(unittest.TestCase):

    def callbackPlayback(self, value):
        self.playback = value

    def test_playback(self):
        self.playback = None
        observable = tl.ObservablePlayback(tl.Playback.Stop)
        observer = tl.PlaybackObserver(observable, self.callbackPlayback)
        self.assertEqual(tl.Playback.Stop, self.playback)
        self.assertTrue(observable.setIfChanged(tl.Playback.Forward))
        self.assertFalse(observable.setIfChanged(tl.Playback.Forward))
        self.assertEqual(tl.Playback.Forward, self.playback)
        observable.setAlways(tl.Playback.Reverse)
        self.assertEqual(tl.Playback.Reverse, self.playback)

    def callbackCacheOptions(self, value):
        self.cacheOptions = value

    def test_cache_options(self):
        self.cacheOptions = None
        observable = tl.ObservablePlayerCacheOptions(tl.PlayerCacheOptions())
        observer = tl.PlayerCacheOptionsObserver(observable, self.callbackCacheOptions)
        self.assertEqual(tl.PlayerCacheOptions(), self.cacheOptions)

    def test_rational_time(self):
        # The opentime values convert between the opentimelineio and
        # tlRenderPy modules only when both are built with the same
        # pybind11 -- this is the test that fails if the superbuild's
        # pin drifts from the version OTIO vendors.
        observable = tl.ObservableRationalTime(otio.opentime.RationalTime(0, 24))
        times = []
        observer = tl.RationalTimeObserver(
            observable,
            lambda value: times.append(value))
        observable.setIfChanged(otio.opentime.RationalTime(12, 24))
        self.assertEqual(
            [otio.opentime.RationalTime(0, 24), otio.opentime.RationalTime(12, 24)],
            times)
        self.assertTrue(hasattr(tl, "ObservablePlayer"))
        self.assertTrue(hasattr(tl, "PlayerObserver"))

class AudioSystemTest(unittest.TestCase):

    def test_members(self):
        context = ftk.Context()
        audioSystem = tl.AudioSystem(context)
        self.assertIsInstance(audioSystem.drivers, list)
        self.assertIsInstance(audioSystem.devices, list)
        audioSystem.defaultDevice

class SystemTest(unittest.TestCase):

    def test_create(self):
        context = ftk.Context()
        system = tl.System(context)
        self.assertIsNotNone(system)

class TimelineTest(unittest.TestCase):

    def setUp(self):
        self.context = ftk.Context()
        tl.init(self.context)

    @staticmethod
    def _writeGapTimeline(dir):
        timeline = otio.schema.Timeline(name="test")
        track = otio.schema.Track(kind=otio.schema.TrackKind.Video)
        timeline.tracks.append(track)
        track.append(otio.schema.Gap(
            source_range=otio.opentime.TimeRange(
                otio.opentime.RationalTime(0, 24),
                otio.opentime.RationalTime(24, 24))))
        fileName = os.path.join(dir, "test.otio")
        otio.adapters.write_to_file(timeline, fileName)
        return fileName

    def test_create(self):
        with tempfile.TemporaryDirectory() as dir:
            fileName = self._writeGapTimeline(dir)
            timeline = tl.Timeline(self.context, fileName)
            self.assertEqual(fileName, timeline.path.get())
            self.assertEqual(
                otio.opentime.RationalTime(24, 24),
                timeline.timeRange.duration)

    def test_player(self):
        with tempfile.TemporaryDirectory() as dir:
            fileName = self._writeGapTimeline(dir)
            timeline = tl.Timeline(self.context, fileName)
            player = tl.Player(self.context, timeline)
            self.assertEqual(24.0, player.defaultSpeed)
            self.assertEqual(24.0, player.speed)
            self.assertEqual(tl.Playback.Stop, player.playback)
            self.assertTrue(player.isStopped)
            self.assertEqual(tl.Loop.Loop, player.loop)
            player.loop = tl.Loop.Once
            self.assertEqual(tl.Loop.Once, player.loop)
            self.assertEqual(1.0, player.volume)
            self.assertFalse(player.mute)

            times = []
            observer = tl.RationalTimeObserver(
                player.observeCurrentTime,
                lambda value: times.append(value))
            player.currentTime = otio.opentime.RationalTime(12, 24)
            self.assertEqual(otio.opentime.RationalTime(12, 24), player.currentTime)
            self.assertEqual(otio.opentime.RationalTime(12, 24), times[-1])
            player.inOutRange = otio.opentime.TimeRange(
                otio.opentime.RationalTime(6, 24),
                otio.opentime.RationalTime(12, 24))
            self.assertEqual(
                otio.opentime.RationalTime(6, 24),
                player.inOutRange.start_time)

if __name__ == '__main__':
    unittest.main()
