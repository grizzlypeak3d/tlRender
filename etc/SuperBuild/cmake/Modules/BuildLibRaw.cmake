include(ExternalProject)

# LibRaw's own build is autotools; the LibRaw project maintains the CMake
# build separately, so the source and the build system are two checkouts,
# with LIBRAW_PATH pointing the second at the first.
set(LibRaw_GIT_REPOSITORY "https://github.com/LibRaw/LibRaw.git")
set(LibRaw_GIT_TAG "0.22.2")
set(LibRaw_cmake_GIT_REPOSITORY "https://github.com/LibRaw/LibRaw-cmake.git")
set(LibRaw_cmake_GIT_TAG "eb98e4325aef2ce85d2eb031c2ff18640ca616d3")

ExternalProject_Add(
    LibRaw-source
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/LibRaw-source
    GIT_REPOSITORY ${LibRaw_GIT_REPOSITORY}
    GIT_TAG ${LibRaw_GIT_TAG}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

ExternalProject_Get_Property(LibRaw-source SOURCE_DIR)

set(LibRaw_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    # LGPL, so a shared library for the same reason FFmpeg is one.
    -DBUILD_SHARED_LIBS=ON
    -DLIBRAW_PATH=${SOURCE_DIR}
    -DENABLE_EXAMPLES=OFF
    -DENABLE_OPENMP=OFF
    -DENABLE_LCMS=OFF
    -DENABLE_JASPER=OFF
    -DLIBRAW_INSTALL=ON)

ExternalProject_Add(
    LibRaw
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/LibRaw
    DEPENDS LibRaw-source
    GIT_REPOSITORY ${LibRaw_cmake_GIT_REPOSITORY}
    GIT_TAG ${LibRaw_cmake_GIT_TAG}
    LIST_SEPARATOR |
    CMAKE_ARGS ${LibRaw_ARGS})
