cmake_minimum_required(VERSION 3.31)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0114 NEW)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(tlRender-SuperBuild)

#-------------------------------------------------------------------------------
# Build options

set(TLRENDER_OCIO ON CACHE BOOL "Build OpenColorIO")
set(TLRENDER_JPEG ON CACHE BOOL "Build JPEG")
set(TLRENDER_TIFF ON CACHE BOOL "Build TIFF")
set(TLRENDER_EXR ON CACHE BOOL "Build OpenEXR")
# The assembly heavy codecs need NASM. It is built from source below on the
# platforms that can, and installed beforehand on Windows -- the README asks
# for it there, as libjpeg-turbo and FFmpeg want it too -- so the codecs find
# it on PATH either way.
set(TLRENDER_AOM ON CACHE BOOL "Build AOM")
set(TLRENDER_SVTAV1 ON CACHE BOOL "Build SVT-AV1")
set(TLRENDER_FFMPEG ON CACHE BOOL "Build FFmpeg (required for audio, see the README for building FFmpeg on Windows)")
set(TLRENDER_FFMPEG_MINIMAL OFF CACHE BOOL "Enable a minimal set of FFmpeg codecs")
set(TLRENDER_SUBPROCESS ON CACHE BOOL "Build subprocess")
if((TLRENDER_AOM OR TLRENDER_SVTAV1 OR TLRENDER_JPEG OR
    TLRENDER_FFMPEG) AND NOT WIN32 AND NOT BSD)
    set(TLRENDER_NASM_DEFAULT ON)
else()
    set(TLRENDER_NASM_DEFAULT OFF)
endif()
set(TLRENDER_NASM ${TLRENDER_NASM_DEFAULT} CACHE BOOL "Build NASM")
set(TLRENDER_OIIO ON CACHE BOOL "Build OpenImageIO")
set(TLRENDER_LIBRAW ON CACHE BOOL "Build LibRaw, for the camera raw formats in OpenImageIO")
set(TLRENDER_PYTHON OFF CACHE BOOL "Build support for Python")
if(APPLE)
    set(TLRENDER_IGNORE_PREFIX_PATH_DEFAULT /opt/homebrew)
endif()
set(TLRENDER_IGNORE_PREFIX_PATH ${TLRENDER_IGNORE_PREFIX_PATH_DEFAULT} CACHE STRING "Ignore the given prefix path")

#-------------------------------------------------------------------------------
# Internal configuration

# \bug Add the OpenColorIO cmake modules.
list(PREPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/../../cmake/Modules
    ${PROJECT_SOURCE_DIR}/cmake/Modules
    ${CMAKE_INSTALL_PREFIX}/share/OpenColorIO/cmake/modules)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF)
endif()

if(NOT CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(TLRENDER_IGNORE_PREFIX_PATH)
    set(CMAKE_IGNORE_PREFIX_PATH ${TLRENDER_IGNORE_PREFIX_PATH})
endif()

if(APPLE)
    set(CMAKE_FIND_FRAMEWORK LAST)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
endif()

# https://stackoverflow.com/questions/45414507/pass-a-list-of-prefix-paths-to-externalproject-add-in-cmake-args
string(REPLACE ";" "|" CMAKE_MODULE_PATH_TMP "${CMAKE_MODULE_PATH}")
string(REPLACE ";" "|" CMAKE_PREFIX_PATH_TMP "${CMAKE_PREFIX_PATH}")

# Common arguments for building external projects.
set(TLRENDER_EXTERNAL_ARGS
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_TMP}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_TMP}
    -DCMAKE_IGNORE_PREFIX_PATH=${CMAKE_IGNORE_PREFIX_PATH}
    -DCMAKE_INSTALL_INCLUDEDIR=include
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_INSTALL_BINDIR=bin
    -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW
    -DCMAKE_POLICY_DEFAULT_CMP0114:STRING=NEW
    -DCMAKE_FIND_FRAMEWORK=${CMAKE_FIND_FRAMEWORK}
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
    -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
    -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
    -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE})
string(REPLACE ";" " " TLRENDER_EXTERNAL_ARGS_TMP "${TLRENDER_EXTERNAL_ARGS}")
message("TLRENDER_EXTERNAL_ARGS: " ${TLRENDER_EXTERNAL_ARGS_TMP})

#-------------------------------------------------------------------------------
# Dependencies

find_package(Threads REQUIRED)

include(Buildminizip-ng)
include(BuildImath)
include(BuildOTIO)
if(TLRENDER_NASM)
    include(BuildNASM)
endif()
if(TLRENDER_OCIO)
    include(Buildyaml-cpp)
    include(Buildexpat)
    include(Buildpystring)
    include(BuildOpenColorIO)
endif()
if(TLRENDER_JPEG)
    include(Buildlibjpeg-turbo)
endif()
if(TLRENDER_TIFF)
    include(BuildTIFF)
endif()
if(TLRENDER_EXR)
    include(BuildOpenJPH)
    include(BuildOpenEXR)
endif()
if(TLRENDER_FFMPEG)
    if(TLRENDER_AOM)
        include(Buildaom)
    endif()
    if(TLRENDER_SVTAV1)
        include(Buildsvt-av1)
    endif()
    include(BuildFFmpeg)
endif()
if(TLRENDER_SUBPROCESS)
    include(Buildsubprocess)
endif()
if(TLRENDER_OIIO)
    if(TLRENDER_LIBRAW)
        include(BuildLibRaw)
    endif()
    include(BuildOpenImageIO)
endif()
