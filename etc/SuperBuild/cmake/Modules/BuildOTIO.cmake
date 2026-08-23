include(ExternalProject)

find_package(Git REQUIRED)

set(OTIO_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git")
# "Add core C++ support for otioz and otiod, take 2 (#2021)", which also adds
# bundle support for multiple media references and image sequences. Newer than
# v0.18.1, which does not have it.
set(OTIO_GIT_TAG "0eebd211b2055f111e2c53d04b5581adc594c1fc")

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
    # one, and it asks by a different name: OTIO calls find_package(Python)
    # where feather-tk, tlRender and DJV call find_package(Python3), and the
    # two read their own ROOT_DIR. Left to find its own OTIO can pick another
    # interpreter, and on Windows a Debug build then stops at the link:
    #
    #     LINK : fatal error LNK1104: cannot open file 'python313.lib'
    #
    # pybind11 undefines _DEBUG around Python.h, so the module is compiled
    # against the release ABI while CMake, configuring Debug, links the debug
    # import library by full path. pyconfig.h then asks for the release one by
    # bare name, with no directory to find it on. An interpreter that ships no
    # debug library never gets into this. Empty here is no argument at all,
    # which is the ordinary case and what continuous integration does.
    -DPython_ROOT_DIR=${Python3_ROOT_DIR}
    -DOTIO_PYTHON_INSTALL=${TLRENDER_PYTHON})

# OTIO is patched, with two changes; see the notes in the patch itself.
#
# The first has it link whichever minizip-ng target is present rather than
# assuming the one from the compatibility layer. Without it OTIO cannot be
# built against the super build's minizip-ng, which is built without that
# layer.
#
# The second is what a shared build on Windows needs. OTIO_EXPORTS and
# OPENTIME_EXPORTS become PRIVATE rather than PUBLIC, so a consumer's headers
# declare the API dllimport instead of dllexport, and the members that had no
# OTIO_API on them get it -- OTIO_API_TYPE on the class is empty on Windows,
# where only the per-member marking carries the declspec. Two source files that
# define exported functions without including the header that marks them are
# given the include as well.
#
# A patch rather than whole file copies: it is smaller, it reads as the change
# it makes, and moving OTIO_GIT_TAG stops the build instead of silently
# dropping whatever upstream changed in the files. git is what applies it,
# which the clone above needs anyway, so nothing new is asked of the machine --
# the patch program itself is not on Windows.
#
# It goes away once these are upstream.
ExternalProject_Add(
    OTIO
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OTIO
    DEPENDS Imath minizip-ng
    GIT_REPOSITORY ${OTIO_GIT_REPOSITORY}
    GIT_TAG ${OTIO_GIT_TAG}
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DOTIO_SOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/OTIO/src/OTIO
        -DOTIO_PATCH=${CMAKE_CURRENT_SOURCE_DIR}/OTIO-patch/otio.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/OTIOApplyPatch.cmake
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
