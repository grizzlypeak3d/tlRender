# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

import tlRenderPy as tl

import unittest

class HDRTest(unittest.TestCase):

    def test_members(self):
        data = tl.HDRData()
        self.assertEqual(tl.HDR_EOTF.SDR, data.eotf)
        self.assertEqual(4, len(data.primaries))
        data.eotf = tl.HDR_EOTF.ST2084
        self.assertEqual(tl.HDR_EOTF.ST2084, data.eotf)
        data.maxCLL = 4000.0
        self.assertEqual(4000.0, data.maxCLL)
        data.maxFALL = 500.0
        self.assertEqual(500.0, data.maxFALL)

    def test_enums(self):
        self.assertNotEqual(tl.HDR_EOTF.SDR, tl.HDR_EOTF.HDR)
        self.assertNotEqual(tl.HDRPrimaries.Red, tl.HDRPrimaries.White)

    def test_operators(self):
        a = tl.HDRData()
        b = tl.HDRData()
        self.assertEqual(a, b)
        b.eotf = tl.HDR_EOTF.HDR
        self.assertNotEqual(a, b)

class URLTest(unittest.TestCase):

    def test_scheme(self):
        self.assertEqual("file://", tl.getURLScheme("file:///tmp/test.mov"))
        self.assertEqual("https://", tl.getURLScheme("https://example.com"))
        self.assertEqual("", tl.getURLScheme("/tmp/test.mov"))

    def test_round_trip(self):
        url = "file:///tmp/render layer/test.mov"
        encoded = tl.encodeURL(url)
        self.assertEqual("file:///tmp/render%20layer/test.mov", encoded)
        self.assertEqual(url, tl.decodeURL(encoded))
        self.assertEqual("a b", tl.decodeURL("a%20b"))

class AudioResampleTest(unittest.TestCase):

    def test_members(self):
        inputInfo = tl.AudioInfo()
        inputInfo.channelCount = 2
        inputInfo.type = tl.AudioType.F32
        inputInfo.sampleRate = 48000
        outputInfo = tl.AudioInfo()
        outputInfo.channelCount = 1
        outputInfo.type = tl.AudioType.S16
        outputInfo.sampleRate = 44100
        resample = tl.AudioResample(inputInfo, outputInfo)
        self.assertEqual(inputInfo, resample.inputInfo)
        self.assertEqual(outputInfo, resample.outputInfo)

if __name__ == '__main__':
    unittest.main()
