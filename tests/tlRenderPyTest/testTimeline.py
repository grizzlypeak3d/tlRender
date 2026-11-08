# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

import feather_tk as ftk
import tlrender as tl

import opentimelineio as otio

import os
import struct
import tempfile
import time
import unittest
import zlib

def writePNG(fileName, rows):
    """An 8-bit RGB PNG from rows of (r, g, b), top row first, written here
    rather than by the library under test: what the file says is up is up."""
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\0" + bytes(c for px in row for c in px) for row in rows)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + \
            struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    with open(fileName, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))

def readPNG(fileName):
    """The rows of an 8-bit RGB or RGBA PNG, top row first, as lists of
    pixel tuples; decoded here, so the file is read as any viewer reads it."""
    data = open(fileName, "rb").read()
    pos, idat = 8, b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    channels = {2: 3, 6: 4}[color]
    raw, stride = zlib.decompress(idat), w * channels
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b, c = prev[i], (prev[i - channels] if i >= channels else 0)
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * channels:x * channels + 3]) for x in range(w)])
        prev = line
    return rows

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
        clippingWarning = tl.ClippingWarning()
        self.assertEqual(0.0, clippingWarning.low)
        self.assertEqual(1.0, clippingWarning.high)
        clippingWarning.enabled = True
        clippingWarning.high = 0.9
        options.clippingWarning = clippingWarning
        self.assertEqual(clippingWarning, options.clippingWarning)
        self.assertNotEqual(clippingWarning, tl.ClippingWarning())

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
        # tlrender modules by value, through opentimelineio's Python API
        # (see OTIOCasters.h), so this holds for any opentimelineio.
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

    def test_pixels_round_trip(self):
        # Pixels made in Python, written to a file, and read back through a
        # player, which is everything a script checking frames needs.
        with tempfile.TemporaryDirectory() as dir:
            image = ftk.Image(8, 4, ftk.ImageType.RGBA_U8)
            image.zero()
            # Opaque: a file need not keep the colour of a pixel nothing can
            # see.
            pixels = image.data
            for x in range(8):
                for y in range(4):
                    pixels[y, x, 3] = 255
            pixels[0, 0, 0] = 255
            pixels[3, 7, 1] = 128

            fileName = os.path.join(dir, "pixels.png")
            info = tl.IOInfo()
            info.video = [image.info]
            writer = tl.WriteSystem(self.context).write(ftk.Path(fileName), info)
            writer.writeVideo(otio.opentime.RationalTime(0, 24), image)
            writer.finish()

            timeline = tl.Timeline(self.context, fileName)
            player = tl.Player(self.context, timeline)
            start = timeline.timeRange.start_time
            player.currentTime = start
            read = None
            for i in range(500):
                # Nothing reaches currentVideo without the context's tick,
                # and until it does the frame there may be an earlier one.
                self.context.tick()
                frames = player.currentVideo
                if frames and frames[0].time == start and frames[0].layers and \
                    frames[0].layers[0].image is not None and \
                    frames[0].layers[0].image.valid:
                    read = frames[0].layers[0].image
                    break
                time.sleep(0.01)
            self.assertIsNotNone(read)
            self.assertEqual(read.size, image.size)
            data = read.data
            channels = data.shape[2]
            # The right way up, whichever way the reader stored the rows.
            self.assertEqual(data[0, 0, 0], 255)
            self.assertEqual(data[3, 7, 1], 128)
            self.assertEqual(data[3, 7, 0], 0)
            self.assertEqual(data[1, 1, 0], 0)
            if channels == 4:
                self.assertEqual(data[1, 1, 3], 255)

    def test_sequence_expand(self):
        # One frame of a sequence opens that frame alone, and expandSeq
        # finds the rest, as the docs say.
        with tempfile.TemporaryDirectory() as dir:
            for i in range(3):
                writePNG(os.path.join(dir, "render.%04d.png" % (i + 1)),
                    [[(i * 100, 0, 0)]])
            path = ftk.Path(os.path.join(dir, "render.0001.png"))
            timeline = tl.Timeline(self.context, path)
            self.assertEqual(1, timeline.timeRange.duration.value)
            timeline = tl.Timeline(self.context,
                ftk.expandSeq(path, ftk.PathOptions()))
            self.assertEqual(
                otio.opentime.TimeRange(
                    otio.opentime.RationalTime(1, 24),
                    otio.opentime.RationalTime(3, 24)),
                timeline.timeRange)

    def test_pixels_upright(self):
        # Which way up the pixels are, against files made and read without
        # the library: row zero of the view is the top of the picture, read
        # or written.
        red, blue = (255, 0, 0), (0, 0, 255)
        with tempfile.TemporaryDirectory() as dir:
            fileName = os.path.join(dir, "upright.png")
            writePNG(fileName, [[red, red], [blue, blue]])
            timeline = tl.Timeline(self.context, fileName)
            player = tl.Player(self.context, timeline)
            start = timeline.timeRange.start_time
            player.currentTime = start
            read = None
            for i in range(500):
                self.context.tick()
                frames = player.currentVideo
                if frames and frames[0].time == start and frames[0].layers and \
                    frames[0].layers[0].image is not None and \
                    frames[0].layers[0].image.valid:
                    read = frames[0].layers[0].image
                    break
                time.sleep(0.01)
            self.assertIsNotNone(read)
            data = read.data
            self.assertEqual((data[0, 0, 0], data[0, 0, 2]), (255, 0))
            self.assertEqual((data[1, 0, 0], data[1, 0, 2]), (0, 255))

            image = ftk.Image(2, 2, ftk.ImageType.RGB_U8)
            pixels = image.data
            for x in range(2):
                for c in range(3):
                    pixels[0, x, c] = red[c]
                    pixels[1, x, c] = blue[c]
            fileName = os.path.join(dir, "written.png")
            info = tl.IOInfo()
            info.video = [image.info]
            writer = tl.WriteSystem(self.context).write(ftk.Path(fileName), info)
            writer.writeVideo(otio.opentime.RationalTime(0, 24), image)
            writer.finish()
            rows = readPNG(fileName)
            self.assertEqual(rows[0][0], red)
            self.assertEqual(rows[1][0], blue)

if __name__ == '__main__':
    unittest.main()
