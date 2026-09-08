# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the feather-tk project.

import ftkPy
import tlRenderPy as tl

import unittest

# Bitmasks: the values are flags, not a run.
BITMASKS = { "FileType" }

class EnumsTest(unittest.TestCase):

    def test_contiguous(self):
        """
        Every bound enum covers 0 through N-1 with no gaps: a gap means
        a value was added to the C++ enum and not to a hand-written
        binding (this is how Key was found missing F13-F24).
        """
        seen = set()
        def walk(mod, prefix):
            for name in dir(mod):
                obj = getattr(mod, name)
                if isinstance(obj, type) and hasattr(obj, "__members__"):
                    if id(obj) in seen or name in BITMASKS:
                        continue
                    seen.add(id(obj))
                    values = sorted(int(v.value) for v in obj.__members__.values())
                    self.assertEqual(
                        values,
                        list(range(len(values))),
                        f"{prefix}.{name} has gaps: {values}")
                elif isinstance(obj, type(tl)) and obj.__name__.startswith("tlRenderPy"):
                    walk(obj, f"{prefix}.{name}")
        walk(tl, "tl")
        self.assertGreater(len(seen), 0)

if __name__ == "__main__":
    unittest.main()
