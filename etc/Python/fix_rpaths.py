# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

"""Give the libraries in a wheel's dependency prefix relocatable names.

Usage: fix_rpaths.py <prefix> <extra>

Each shared library under <prefix>/lib is made to find the others beside it,
and feather-tk's at <extra>, a path relative to that directory. Whatever
absolute paths the builds recorded -- the prefix itself, as an install name
or an rpath -- are dropped, since they name the machine the wheel was built
on. Nothing is done on Windows, which finds DLLs by directory.

The dependencies come from a dozen build systems that each name their
libraries their own way, which is why this is done afterwards rather than
asked of each of them.
"""

import os
import subprocess
import sys


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def libraries(lib_dir, suffixes):
    for name in sorted(os.listdir(lib_dir)):
        path = os.path.join(lib_dir, name)
        if os.path.islink(path) or not os.path.isfile(path):
            continue
        if any(suffix in name for suffix in suffixes):
            yield path


def fix_macos(prefix, lib_dir, extra):
    wanted = ["@loader_path", "@loader_path/" + extra]
    for path in libraries(lib_dir, (".dylib", ".so")):
        args = []

        # The library's own name and those of its dependencies, when they
        # are absolute paths into the prefix.
        lines = run("otool", "-D", path).splitlines()[1:]
        if lines and lines[0].startswith(prefix):
            args += ["-id", "@rpath/" + os.path.basename(lines[0])]
        for line in run("otool", "-L", path).splitlines()[1:]:
            dep = line.strip().split(" (")[0]
            if dep.startswith(prefix):
                args += ["-change", dep, "@rpath/" + os.path.basename(dep)]

        # Absolute rpaths out, the relative ones in.
        rpaths = []
        lines = run("otool", "-l", path).splitlines()
        for i, line in enumerate(lines):
            if line.strip() == "cmd LC_RPATH":
                rpaths.append(lines[i + 2].strip().split(" ")[1])
        for rpath in dict.fromkeys(rpaths):
            if rpath.startswith("/"):
                args += ["-delete_rpath", rpath]
        for rpath in wanted:
            if rpath not in rpaths:
                args += ["-add_rpath", rpath]

        if args:
            run("install_name_tool", *args, path)
            # The edit invalidates the signature, and Apple silicon will not
            # load an unsigned library; an ad hoc one is what the linker
            # would have given it.
            run("codesign", "--force", "--sign", "-", path)


def fix_linux(lib_dir, extra):
    wanted = ["$ORIGIN", "$ORIGIN/" + extra]
    for path in libraries(lib_dir, (".so",)):
        with open(path, "rb") as f:
            if f.read(4) != b"\x7fELF":
                continue
        current = run("patchelf", "--print-rpath", path).strip()
        rpaths = [r for r in current.split(":") if r and not r.startswith("/")]
        rpaths += [r for r in wanted if r not in rpaths]
        run("patchelf", "--set-rpath", ":".join(rpaths), path)


def main():
    # Both spellings, since a build can record either: on macOS /tmp is a
    # link to /private/tmp.
    prefix = (os.path.abspath(sys.argv[1]), os.path.realpath(sys.argv[1]))
    extra = sys.argv[2]
    lib_dir = os.path.join(prefix[0], "lib")
    if sys.platform == "darwin":
        fix_macos(prefix, lib_dir, extra)
    elif sys.platform.startswith("linux"):
        fix_linux(lib_dir, extra)


if __name__ == "__main__":
    main()
