# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

"""Build a C++ program against the installed tlrender and feather_tk
packages, and run it.

The wheels carry CMake packages so that a project with bindings of its own
can build against the libraries in them. That only works if every path in
those packages names a file in the wheel rather than the machine it was
built on, which nothing in the Python tests would notice. Run by
cibuildwheel after the tests, with cmake from pip.
"""

import os
import shutil
import subprocess
import sys
import tempfile

import feather_tk
import tlrender

HERE = os.path.dirname(os.path.abspath(__file__))


def run(*args, env=None):
    print(" ".join(args), flush=True)
    subprocess.run(args, check=True, env=env)


def main():
    cmake = shutil.which("cmake")
    if not cmake:
        raise RuntimeError("cmake is not on PATH")
    with tempfile.TemporaryDirectory() as build:
        # tlRender alone: its package finds feather-tk's beside it.
        run(cmake, "-S", HERE, "-B", build,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DtlRender_DIR=" + tlrender.get_cmake_dir())
        run(cmake, "--build", build, "--config", "Release")

        env = dict(os.environ)
        exe = os.path.join(build, "wheel-consumer")
        if sys.platform == "win32":
            exe = os.path.join(build, "Release", "wheel-consumer.exe")
            # Windows finds a program's DLLs on PATH: the ones in both
            # packages, and the C++ runtime delvewheel put beside them.
            dirs = []
            for package in (tlrender, feather_tk):
                here = os.path.dirname(package.__file__)
                dirs += [
                    os.path.join(here, "bin"),
                    os.path.join(here, "lib"),
                    here + ".libs"]
            env["PATH"] = os.pathsep.join(dirs + [env.get("PATH", "")])
        run(exe, env=env)


if __name__ == "__main__":
    main()
