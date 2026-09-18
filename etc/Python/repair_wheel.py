# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

"""Repair a tlrender wheel, leaving feather-tk's libraries in feather-tk's.

Usage: repair_wheel.py <wheel> <dest_dir> [<delocate_archs>]

The wheel links the libraries of the feather-tk wheel beside it -- libftkUI,
and the zlib, libpng, FreeType and SDL that came with it -- and names them by
rpath. The repair tools would either fail to find them or copy them in, and
a second copy is exactly what the tlrender wheel is built not to have. So
the feather-tk wheel pyproject.toml pins is installed to a scratch directory,
the tool is shown where its libraries are, and told to leave every one of
them out.

For cibuildwheel's repair-wheel-command.
"""

import os
import subprocess
import sys
import tempfile
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(os.path.dirname(HERE))


def feather_tk_requirement():
    with open(os.path.join(PROJECT, "pyproject.toml"), "rb") as f:
        dependencies = tomllib.load(f)["project"]["dependencies"]
    for dependency in dependencies:
        if dependency.replace("_", "-").startswith("feather-tk"):
            return dependency
    raise RuntimeError("pyproject.toml has no feather-tk dependency")


def run(*args, env=None):
    print(" ".join(args), flush=True)
    subprocess.run(args, check=True, env=env)


def main():
    wheel, dest_dir = sys.argv[1], sys.argv[2]
    archs = sys.argv[3] if len(sys.argv) > 3 else None

    with tempfile.TemporaryDirectory() as scratch:
        run(sys.executable, "-m", "pip", "install", "--no-deps",
            "--target", scratch, feather_tk_requirement())
        package = os.path.join(scratch, "feather_tk")

        if sys.platform == "darwin":
            lib_dir = os.path.join(package, "lib")
            env = dict(os.environ, DYLD_LIBRARY_PATH=lib_dir)
            args = ["delocate-wheel", "-v", "-w", dest_dir,
                    # The rpaths to the feather_tk package are what finds
                    # its libraries once installed.
                    "--no-sanitize-rpaths",
                    "--exclude", package]
            if archs:
                args += ["--require-archs", archs]
            run(*args, wheel, env=env)

        elif sys.platform.startswith("linux"):
            lib_dir = os.path.join(package, "lib")
            env = dict(os.environ, LD_LIBRARY_PATH=lib_dir)
            args = ["auditwheel", "repair", "-w", dest_dir]
            for name in sorted(os.listdir(lib_dir)):
                if ".so" in name:
                    args += ["--exclude", name]
            run(*args, wheel, env=env)

        elif sys.platform == "win32":
            bin_dir = os.path.join(package, "bin")
            names = [n for n in sorted(os.listdir(bin_dir)) if n.endswith(".dll")]
            # What the wheel has already is found through its own bin
            # directory, and feather-tk's through the feather_tk package (see
            # tlrender/__init__.py); only the C++ runtime is added. Its name
            # is left alone, since the other DLLs import it too and are not
            # rewritten.
            run("delvewheel", "repair", "-v", "-w", dest_dir,
                "--ignore-existing",
                "--no-mangle-all",
                "--add-path", bin_dir,
                # os.pathsep: delvewheel splits its lists the way the
                # platform splits PATH, which is ";" on Windows.
                "--exclude", os.pathsep.join(names),
                wheel)

        else:
            raise RuntimeError("No repair for " + sys.platform)


if __name__ == "__main__":
    main()
