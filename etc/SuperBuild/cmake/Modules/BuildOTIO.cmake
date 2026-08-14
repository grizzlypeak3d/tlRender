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
