# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

import ftkPy as ftk
import tlRenderPy as tl

import unittest

class IOTest(unittest.TestCase):

    def test_members(self):
        info = tl.IOInfo()
        self.assertEqual([], info.video)
        self.assertEqual({}, info.tags)

    def test_operators(self):
        self.assertEqual(tl.IOInfo(), tl.IOInfo())
        a = tl.IOInfo()
        a.tags = { "Artist": "Test" }
        self.assertNotEqual(a, tl.IOInfo())

class ReadSystemTest(unittest.TestCase):

    def test_names(self):
        context = ftk.Context()
        readSystem = tl.ReadSystem(context)
        self.assertIn("FFmpeg", readSystem.names)
        self.assertIn("PNG", readSystem.names)
        self.assertIn("OpenEXR", readSystem.names)

    def test_exts(self):
        context = ftk.Context()
        readSystem = tl.ReadSystem(context)
        exts = list(readSystem.getExts())
        self.assertTrue(len(exts) > 0)
        self.assertIn(".exr", exts)

    def test_file_type(self):
        context = ftk.Context()
        readSystem = tl.ReadSystem(context)
        self.assertEqual(tl.FileType.Seq, readSystem.getFileType(".exr"))
        self.assertEqual(tl.FileType.Media, readSystem.getFileType(".mov"))

class WriteSystemTest(unittest.TestCase):

    def test_names(self):
        context = ftk.Context()
        writeSystem = tl.WriteSystem(context)
        self.assertIn("FFmpeg", writeSystem.names)
        self.assertIn("PNG", writeSystem.names)
        self.assertIn("OpenEXR", writeSystem.names)

if __name__ == '__main__':
    unittest.main()
