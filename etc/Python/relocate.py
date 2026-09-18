# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

"""Make an installed wheel prefix independent of the machine it was built on.

Usage: relocate.py <prefix> <deps> <feather_tk>

Run at the end of the install, on the directory the wheel is made from.
<deps> is where the wheel's dependencies were built and <feather_tk> the
feather-tk package they were built against.

Two things are done:

* The dependencies' CMake packages name those two directories by absolute
  path -- OpenTimelineIO's targets, OpenImageIO's and minizip-ng's configs
  -- which names nothing on anyone else's machine. Each is rewritten
  relative to the file it is in: the wheel's own files through
  ${CMAKE_CURRENT_LIST_DIR}, and feather-tk's through the feather_tk
  package installed beside this one.

* A shared library installs as a file and a chain of links to it, and a
  wheel cannot hold links, so each name becomes a copy of its own:
  libOpenEXR.dylib, libOpenEXR-3_4.dylib, libOpenEXR-3_4.33.dylib and the
  file itself. What is kept is every name a binary in the wheel loads it
  by -- its install name or soname, and sometimes another: FFmpeg's
  libraries load each other by a shorter name than their own -- and the
  plain development name, which CMake's find modules look for, as
  OpenImageIO's package finds TIFF and JPEG. The CMake packages that named
  the others name the install name instead.

Windows installs no links, so there only the paths are rewritten.
"""

import os
import re
import shutil
import subprocess
import sys


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def spellings(path):
    """The ways a build can have recorded a directory: as given and with the
    links in it resolved (/tmp is /private/tmp on macOS), with either slash."""
    result = set()
    for p in (os.path.abspath(path), os.path.realpath(path)):
        result.add(p)
        result.add(p.replace("\\", "/"))
    return sorted(result, key=len, reverse=True)


def cmake_files(prefix):
    for root, _, files in os.walk(prefix):
        for name in files:
            if name.endswith(".cmake"):
                yield os.path.join(root, name)


def relocate_paths(prefix, deps, feather_tk):
    for path in cmake_files(prefix):
        with open(path, encoding="utf-8", errors="surrogateescape") as f:
            text = f.read()
        rel = os.path.relpath(prefix, os.path.dirname(path)).replace("\\", "/")
        here = "${CMAKE_CURRENT_LIST_DIR}/" + rel
        new = text
        # feather-tk first: it is not inside the dependency prefix, but its
        # scratch copy sits beside it and shares the prefix's name.
        for spelling in spellings(feather_tk):
            new = new.replace(spelling, here + "/../feather_tk")
        for spelling in spellings(deps):
            new = new.replace(spelling, here)
        if new != text:
            with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
                f.write(new)
            print("relocated", os.path.relpath(path, prefix))


def library_name(path):
    """The name a library is loaded by: its install name or soname."""
    if sys.platform == "darwin":
        lines = run("otool", "-D", path).splitlines()[1:]
        return os.path.basename(lines[0].strip()) if lines else None
    soname = run("patchelf", "--print-soname", path).strip()
    return soname or None


def is_binary(path):
    with open(path, "rb") as f:
        magic = f.read(4)
    return magic == b"\x7fELF" or magic in (
        b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def referenced_names(prefix):
    """The library names the binaries in the prefix load theirs by."""
    names = set()
    for root, _, files in os.walk(prefix):
        for name in files:
            path = os.path.join(root, name)
            if os.path.islink(path) or not is_binary(path):
                continue
            if sys.platform == "darwin":
                for line in run("otool", "-L", path).splitlines()[1:]:
                    names.add(os.path.basename(line.strip().split(" (")[0]))
            else:
                names.update(run("patchelf", "--print-needed", path).split())
    return names


def is_dev_name(name):
    return name.endswith(".dylib") and name.count(".") == 1 or \
        name.endswith(".so") and name.count(".") == 1


def deduplicate(prefix, deps):
    lib_src = os.path.join(deps, "lib")
    lib_dst = os.path.join(prefix, "lib")

    # The chains of links in the dependency prefix, which still has them.
    groups = {}
    for name in os.listdir(lib_src):
        path = os.path.join(lib_src, name)
        if not (".dylib" in name or ".so" in name) or not os.path.isfile(path):
            continue
        groups.setdefault(os.path.realpath(path), set()).add(name)

    # A library can be asked for by more than one name: FFmpeg's libraries
    # load each other as libavcodec.63.dylib, and whatever links them names
    # libavcodec.63.1.101.dylib, the install name. Every name a binary here
    # asks for stays.
    referenced = referenced_names(prefix)

    renames = {}
    kept_names = set()
    for real, names in groups.items():
        if len(names) < 2:
            continue
        keep = library_name(real)
        if keep not in names:
            continue
        # The shortest: libOpenEXR.dylib rather than libOpenEXR-3_4.dylib.
        dev = sorted((n for n in names if is_dev_name(n)), key=len)
        kept = {keep, *dev[:1]} | (names & referenced)
        kept_names |= kept
        for name in names - kept:
            renames[name] = keep

    if not renames:
        return

    # The CMake packages name the copy that stays.
    pattern = re.compile(
        r"(?<=/)(" + "|".join(re.escape(n) for n in sorted(renames, key=len, reverse=True)) + r")(?=[\"'\s;)]|$)")
    for path in cmake_files(prefix):
        with open(path, encoding="utf-8", errors="surrogateescape") as f:
            text = f.read()
        new = pattern.sub(lambda m: renames[m.group(1)], text)
        if new != text:
            with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
                f.write(new)

    # The install is still links at this point -- libOpenEXR-3_4.33.dylib
    # to libOpenEXR-3_4.33.3.4.15.dylib -- so each name kept is made a file
    # of its own before any is removed; otherwise removing the file a kept
    # name points at leaves that name pointing at nothing, and the wheel
    # goes without it.
    for name in kept_names:
        path = os.path.join(lib_dst, name)
        if os.path.islink(path):
            target = os.path.realpath(path)
            os.remove(path)
            shutil.copy2(target, path)

    removed = 0
    for name in renames:
        path = os.path.join(lib_dst, name)
        if os.path.lexists(path):
            os.remove(path)
            removed += 1
    print(f"removed {removed} duplicate library copies")


def main():
    prefix, deps, feather_tk = sys.argv[1:4]
    relocate_paths(prefix, deps, feather_tk)
    if sys.platform != "win32":
        deduplicate(prefix, deps)


if __name__ == "__main__":
    main()
