#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the tlRender project.

"""Frameworks for iOS from the dynamic libraries a super build installed.

An iOS application carries a dynamic library as a framework in its bundle:
the store refuses a bare library. For each library named, this sets the
library's install name to the framework's, "@rpath/<name>.framework/<name>",
rewrites the references the libraries in the group make to each other the
same way, and makes "<prefix>/Frameworks/<name>.framework" with a copy of
the library and the property list a framework needs. The libraries in the
prefix keep the new names too, so that linking against them records the
framework's name, which the bundle's run path then resolves.

    MakeFrameworks.py --prefix P --min-os 17.0 --identifier com.x avcodec raw
"""

import argparse
import os
import plistlib
import re
import shutil
import subprocess


def real_library(lib_dir, name):
    """The library file itself, not one of the symbolic links to it."""
    pattern = re.compile(r'^lib' + re.escape(name) + r'(\.[0-9]+)*\.dylib$')
    files = [f for f in os.listdir(lib_dir) if pattern.match(f)]
    real = [f for f in files if not os.path.islink(os.path.join(lib_dir, f))]
    if not real:
        raise SystemExit(f'No library for {name} in {lib_dir}')
    # The file "lib<name>.dylib" points at, which is the one a link
    # records. Not the longest name: a prefix that had an older release
    # installed keeps its files beside the new ones, the same length, and
    # picking the old one left the new one with its own install name, which
    # the application then linked and the bundle did not have.
    link = os.path.join(lib_dir, f'lib{name}.dylib')
    if os.path.exists(link):
        target = os.path.basename(os.path.realpath(link))
        if target in real:
            return target, files
    if len(real) > 1:
        raise SystemExit(f'More than one library for {name} in {lib_dir}: {real}')
    return real[0], files


def version_of(file_name):
    m = re.search(r'\.([0-9]+(\.[0-9]+)*)\.dylib$', file_name)
    return m.group(1) if m else '1.0'


def dependencies(path):
    out = subprocess.run(['otool', '-L', path], check=True, capture_output=True, text=True).stdout
    deps = []
    for line in out.splitlines()[1:]:
        line = line.strip()
        if line:
            deps.append(line.split(' (')[0])
    return deps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--prefix', required=True)
    ap.add_argument('--min-os', default='17.0')
    ap.add_argument('--identifier', default='org.tlrender')
    ap.add_argument('names', nargs='+')
    args = ap.parse_args()
    lib_dir = os.path.join(args.prefix, 'lib')
    fw_dir = os.path.join(args.prefix, 'Frameworks')
    os.makedirs(fw_dir, exist_ok=True)

    # Every file name a library in the group goes by, to its framework name.
    reals = {}
    aliases = {}
    for name in args.names:
        real, files = real_library(lib_dir, name)
        reals[name] = real
        for f in files:
            aliases[f] = name

    for name, real in reals.items():
        path = os.path.join(lib_dir, real)
        new_id = f'@rpath/{name}.framework/{name}'
        cmd = ['install_name_tool', '-id', new_id]
        for dep in dependencies(path):
            dep_name = aliases.get(os.path.basename(dep))
            if dep_name and dep_name != name:
                cmd += ['-change', dep, f'@rpath/{dep_name}.framework/{dep_name}']
        subprocess.run(cmd + [path], check=True)

        framework = os.path.join(fw_dir, f'{name}.framework')
        if os.path.isdir(framework):
            shutil.rmtree(framework)
        os.makedirs(framework)
        shutil.copy2(path, os.path.join(framework, name))
        version = version_of(real)
        with open(os.path.join(framework, 'Info.plist'), 'wb') as f:
            plistlib.dump({
                'CFBundleDevelopmentRegion': 'en',
                'CFBundleExecutable': name,
                # An identifier has letters, digits, hyphens and periods:
                # the underscore of "raw_r" is refused.
                'CFBundleIdentifier': f"{args.identifier}.{name.replace('_', '-')}",
                'CFBundleInfoDictionaryVersion': '6.0',
                'CFBundleName': name,
                'CFBundlePackageType': 'FMWK',
                'CFBundleShortVersionString': version,
                'CFBundleVersion': version,
                'CFBundleSupportedPlatforms': ['iPhoneOS'],
                'MinimumOSVersion': args.min_os,
            }, f)
        print(f'{real} -> Frameworks/{name}.framework')


if __name__ == '__main__':
    main()
