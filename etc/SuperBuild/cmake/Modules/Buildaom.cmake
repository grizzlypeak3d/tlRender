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
    -DENABLE_DOCS=0)
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
    LIST_SEPARATOR |
    CMAKE_ARGS ${aom_ARGS})
