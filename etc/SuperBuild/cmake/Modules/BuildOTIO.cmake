include(ExternalProject)

find_package(Git REQUIRED)

set(OTIO_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git")
# "Fixes for API exports (#2039)", the last of the changes this build carried
# as a patch. Newer than v0.18.1, which has neither that nor "Add core C++
# support for otioz and otiod, take 2 (#2021)" -- bundles, and bundle support
# for multiple media references and image sequences.
set(OTIO_GIT_TAG "64bb3cd3d2d1")

set(OTIO_SHARED_LIBS ON)
if(NOT BUILD_SHARED_LIBS)
    set(OTIO_SHARED_LIBS OFF)
endif()

set(OTIO_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOTIO_FIND_IMATH=ON
    # Use the minizip-ng and zlib from the super build; without this OTIO
    # builds its own copies of both for its otioz and otiod support. This
    # depends on the patch below.
    -DOTIO_FIND_MINIZIP_NG=ON
    -DOTIO_SHARED_LIBS=${OTIO_SHARED_LIBS}
    # The same interpreter the rest of the build was pointed at. OTIO is an
    # external project, so it does not inherit the cache entry that says which
    # one. OTIO, feather-tk, tlRender and DJV all call find_package(Python),
    # so Python_ROOT_DIR is the one hint for all of them. Left to find its own
    # OTIO can pick another interpreter, and on Windows a Debug build then
    # stops at the link:
    #
    #     LINK : fatal error LNK1104: cannot open file 'python313.lib'
    #
    # pybind11 undefines _DEBUG around Python.h, so the module is compiled
    # against the release ABI while CMake, configuring Debug, links the debug
    # import library by full path. pyconfig.h then asks for the release one by
    # bare name, with no directory to find it on. An interpreter that ships no
    # debug library never gets into this. Empty here is no argument at all,
    # which is the ordinary case and what continuous integration does.
    -DPython_ROOT_DIR=${Python_ROOT_DIR}
    -DOTIO_PYTHON_INSTALL=${TLRENDER_PYTHON})

# On Linux the loader looks for a library's dependencies with that library's
# own rpath, not with the rpaths of whatever loaded it. The Python module
# finds libopentimelineio beside itself by "$ORIGIN", and libopentimelineio
# then has nowhere to look for minizip-ng and Imath:
#
#     ImportError: libminizip-ng.so.4: cannot open shared object file
#
# A program linking the library has loaded those itself already, which is why
# only the import fails. The second entry reaches the prefix's lib from the
# Python package; the first is the library alone in lib. Relative, so the
# install stays relocatable. macOS resolves with the loading module's rpaths
# and is handled in OTIOInstallNames.cmake.
if(UNIX AND NOT APPLE)
    list(APPEND OTIO_ARGS "-DCMAKE_INSTALL_RPATH=$ORIGIN|$ORIGIN/../../lib")
endif()

# OTIO is patched, with two changes; see the notes in the patch itself.
#
# The first drops the "_d" debug postfix, which hides the Python modules from
# the release interpreter that runs the tests.
#
# The second keeps the type info of its types visible in a static build, so
# that the any values it hands out can be cast here: upstream hides its
# symbols, and the types are marked for export only in a shared build, so
# this side was left making a second copy of that type info and the casts
# stopped matching.
#
# A patch rather than whole file copies: it is smaller, it reads as the change
# it makes, and moving OTIO_GIT_TAG stops the build instead of silently
# dropping whatever upstream changed in the files. git is what applies it,
# which the clone above needs anyway, so nothing new is asked of the machine --
# the patch program itself is not on Windows.
#
# The first goes away once it is upstream; see OTIO PR #2040.
ExternalProject_Add(
    OTIO
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OTIO
    DEPENDS Imath minizip-ng
    GIT_REPOSITORY ${OTIO_GIT_REPOSITORY}
    GIT_TAG ${OTIO_GIT_TAG}
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DPATCH_SOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/OTIO/src/OTIO
        -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/OTIO-patch/otio.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake
    LIST_SEPARATOR |
    CMAKE_ARGS ${OTIO_ARGS})

# Building the Python bindings puts OTIO's own libraries inside its Python
# package rather than in lib, which is what the package needs -- the module
# and the libraries sit in one directory -- and leaves everything else unable
# to find them. On Linux the loader simply does not have them on its path:
#
#     ImportError: libopentimelineio.so.19: cannot open shared object file
#
# macOS has that and one more: OTIO names those libraries "@loader_path/...",
# so every program that links them records that name and looks beside itself
# rather than where they are. Naming them by rpath is what those programs
# already expect, since they carry the prefix's lib in theirs.
#
# So both platforms get the libraries linked into lib, and macOS gets the
# names rewritten as well. They are linked rather than copied so that a
# process loading both the Python package and ours still has one of each:
# two images of libopentimelineio do not share their type information, which
# is the thing shared libraries were turned on to avoid.
#
# What libopentimelineio says about libopentime is left alone: it resolves
# beside libopentimelineio, where the file is, and the Python module has no
# rpath of its own to fall back on if it were changed.
if(OTIO_SHARED_LIBS)
    set(OTIO_IS_APPLE OFF)
    if(APPLE)
        set(OTIO_IS_APPLE ON)
    endif()
    if(TLRENDER_PYTHON)
        set(OTIO_DYLIB_DIR ${CMAKE_INSTALL_PREFIX}/python/opentimelineio)
    else()
        set(OTIO_DYLIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
    endif()
    ExternalProject_Add_Step(
        OTIO OTIO-install-names
        COMMAND ${CMAKE_COMMAND}
            -DOTIO_DYLIB_DIR=${OTIO_DYLIB_DIR}
            -DOTIO_LIB_DIR=${CMAKE_INSTALL_PREFIX}/lib
            -DOTIO_APPLE=${OTIO_IS_APPLE}
            -P ${CMAKE_CURRENT_LIST_DIR}/OTIOInstallNames.cmake
        DEPENDEES install
        ALWAYS OFF)
endif()
