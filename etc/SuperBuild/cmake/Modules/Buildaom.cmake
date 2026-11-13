include(ExternalProject)

set(aom_GIT_REPOSITORY "https://aomedia.googlesource.com/aom.git")
set(aom_GIT_TAG "v3.13.3")

set(aom_DEPS)
if(TLRENDER_NASM)
    list(APPEND aom_DEPS NASM)
endif()

set(aom_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DENABLE_EXAMPLES=0
    -DENABLE_TESTS=0
    -DENABLE_TOOLS=0
    -DENABLE_DOCS=0
    # Only the decoder: FFmpeg reads AV1 with it, and writes AV1 with
    # SVT-AV1 (Buildsvt-av1.cmake). Without the encoder the library is a
    # third of the size and builds in 60% of the time.
    -DCONFIG_AV1_ENCODER=0)
if(TLRENDER_NASM)
    list(APPEND aom_ARGS -DCMAKE_ASM_NASM_COMPILER=${CMAKE_INSTALL_PREFIX}/bin/nasm)
endif()
# Building for the other macOS architecture: aom chooses its assembly from
# the processor CMake reports, which is the one doing the building.
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(APPEND aom_ARGS -DAOM_TARGET_CPU=${CMAKE_OSX_ARCHITECTURES})
endif()

ExternalProject_Add(
    aom
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/aom
    DEPENDS ${aom_DEPS}
    GIT_REPOSITORY ${aom_GIT_REPOSITORY}
    GIT_TAG ${aom_GIT_TAG}
    # The tag alone: the whole history is hundreds of megabytes from a
    # server that dropped the transfer part way on the Windows runners,
    # and a clone that fails once fails the build there even when the
    # retry succeeds, since MSBuild reads the first attempt's error.
    GIT_SHALLOW TRUE
    LIST_SEPARATOR |
    CMAKE_ARGS ${aom_ARGS})
