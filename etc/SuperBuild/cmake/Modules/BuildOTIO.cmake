include(ExternalProject)

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
    -DOTIO_PYTHON_INSTALL=${TLRENDER_PYTHON})

# The patched files are copies of the ones from OTIO_GIT_TAG that link
# whichever minizip-ng target is present instead of assuming the one from the
# compatibility layer; see the notes in them. Without this OTIO cannot be
# built against the super build's minizip-ng, which is built without that
# layer.
#
# Because these are whole-file copies, moving OTIO_GIT_TAG silently reverts
# whatever else upstream changed in them. Re-copy all three from the new
# source and re-apply the change. Watch the top-level CMakeLists.txt in
# particular: it is where the target is chosen, and it sees far more unrelated
# churn than the other two. They can be dropped once the change is upstream.
ExternalProject_Add(
    OTIO
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OTIO
    DEPENDS Imath minizip-ng
    GIT_REPOSITORY ${OTIO_GIT_REPOSITORY}
    GIT_TAG ${OTIO_GIT_TAG}
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/OTIO-patch/CMakeLists.txt
        ${CMAKE_CURRENT_BINARY_DIR}/OTIO/src/OTIO/CMakeLists.txt
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/OTIO-patch/src/opentimelineio/CMakeLists.txt
        ${CMAKE_CURRENT_BINARY_DIR}/OTIO/src/OTIO/src/opentimelineio/CMakeLists.txt
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/OTIO-patch/tests/CMakeLists.txt
        ${CMAKE_CURRENT_BINARY_DIR}/OTIO/src/OTIO/tests/CMakeLists.txt
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
