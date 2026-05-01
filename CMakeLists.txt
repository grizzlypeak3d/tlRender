cmake_minimum_required(VERSION 3.31)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0114 NEW)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(TLRENDER_VERSION_MAJOR "0")
set(TLRENDER_VERSION_MINOR "19")
set(TLRENDER_VERSION_PATCH "0")
set(TLRENDER_VERSION_DEV "-dev")
set(TLRENDER_VERSION_FULL ${TLRENDER_VERSION_MAJOR}.${TLRENDER_VERSION_MINOR}.${TLRENDER_VERSION_PATCH}${TLRENDER_VERSION_DEV})
add_definitions(-DTLRENDER_VERSION_MAJOR=${TLRENDER_VERSION_MAJOR})
add_definitions(-DTLRENDER_VERSION_MINOR=${TLRENDER_VERSION_MINOR})
add_definitions(-DTLRENDER_VERSION_PATCH=${TLRENDER_VERSION_PATCH})
add_definitions(-DTLRENDER_VERSION_DEV="${TLRENDER_VERSION_DEV}")
add_definitions(-DTLRENDER_VERSION_FULL="${TLRENDER_VERSION_FULL}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
include(tlRenderVcpkg)

project(
    tlRender
    VERSION ${TLRENDER_VERSION_MAJOR}.${TLRENDER_VERSION_MINOR}.${TLRENDER_VERSION_PATCH}
    DESCRIPTION "tlRender is an open source library for building playback and review applications for visual effects, film, and animation"
    HOMEPAGE_URL "https://github.com/grizzlypeak3d/tlRender"
    LANGUAGES CXX C)

#-------------------------------------------------------------------------------
# Build options

set(ftk_API "GL_4_1" CACHE STRING "Graphics API (GL_4_1, GL_4_1_Debug, GLES_2)")

set(TLRENDER_NET OFF CACHE BOOL "Enable network support")
set(TLRENDER_OCIO ON CACHE BOOL "Enable support for OpenColorIO")
set(TLRENDER_SDL2 ON CACHE BOOL "Enable support for SDL2")
set(TLRENDER_SDL3 OFF CACHE BOOL "Enable support for SDL3")
set(TLRENDER_JPEG ON CACHE BOOL "Enable support for JPEG I/O")
set(TLRENDER_TIFF ON CACHE BOOL "Enable support for TIFF I/O")
set(TLRENDER_PNG ON CACHE BOOL "Enable support for PNG I/O")
set(TLRENDER_EXR ON CACHE BOOL "Enable support for EXR I/O")
set(TLRENDER_FFMPEG ON CACHE BOOL "Enable support for FFmpeg (required for audio)")
set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "Enable support for FFmpeg I/O (required for codecs)")
set(TLRENDER_OIIO ON CACHE BOOL "Enable support for OpenImageIO")
set(TLRENDER_USD OFF CACHE BOOL "Enable support for USD")
set(TLRENDER_WMF ${WIN32} CACHE BOOL "Enable support for Windows Media Foundation")
set(TLRENDER_AVF OFF CACHE BOOL "Enable support for AVFoundation (macOS)")
set(TLRENDER_BMD OFF CACHE BOOL "Enable support for Blackmagic Design devices")
set(TLRENDER_BMD_SDK "" CACHE PATH "Full path to the Blackmagic Design SDK")
set(TLRENDER_QT6 OFF CACHE BOOL "Enable support for Qt6")
set(TLRENDER_PYTHON OFF CACHE BOOL "Enable support for Python")
set(TLRENDER_PROGRAMS ON CACHE BOOL "Build programs")
set(TLRENDER_EXAMPLES ON CACHE BOOL "Build examples")
set(TLRENDER_TESTS ON CACHE BOOL "Build tests")
if(APPLE)
    set(TLRENDER_IGNORE_PREFIX_PATH_DEFAULT /opt/homebrew)
endif()
set(TLRENDER_IGNORE_PREFIX_PATH ${TLRENDER_IGNORE_PREFIX_PATH_DEFAULT} CACHE STRING "Ignore the given prefix path")
set(TLRENDER_GCOV OFF CACHE BOOL "Enable gcov code coverage")
set(TLRENDER_GPROF OFF CACHE BOOL "Enable gprof code profiling")

#-------------------------------------------------------------------------------
# Internal configuration

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF)
endif()

if(NOT CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(TLRENDER_BMD)
    add_definitions(-DTLRENDER_BMD)
endif()

if(TLRENDER_QT6)
    add_definitions(-DTLRENDER_QT6)
endif()

if(TLRENDER_IGNORE_PREFIX_PATH)
    set(CMAKE_IGNORE_PREFIX_PATH ${TLRENDER_IGNORE_PREFIX_PATH})
endif()

if(CMAKE_BUILD_TYPE MATCHES "^Debug$")
    add_definitions(-DFTK_ASSERT)
endif()

if(TLRENDER_TESTS)
    add_definitions(-DTLRENDER_SAMPLE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/etc/SampleData")
    set(CTEST_OUTPUT_ON_FAILURE ON)
    enable_testing()
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/deps
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/etc
    ${PROJECT_SOURCE_DIR}/tests)

if(TLRENDER_GPROF)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif()

if(TLRENDER_GCOV)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-update=atomic")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-update=atomic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -fprofile-update=atomic")
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
endif()

if(APPLE)
    set(CMAKE_FIND_FRAMEWORK LAST)
endif()

if(WIN32)
elseif(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
    # \bug Should this be set automatically?
    if(CMAKE_BUILD_TYPE MATCHES "^Debug$")
        set(CMAKE_BUILD_RPATH "@executable_path/../../../install-Debug/lib")
    elseif (CMAKE_BUILD_TYPE MATCHES "^RelWithDebInfo$")
        set(CMAKE_BUILD_RPATH "@executable_path/../../../install-RelWithDebInfo/lib")
    elseif (CMAKE_BUILD_TYPE MATCHES "^Release$")
        set(CMAKE_BUILD_RPATH "@executable_path/../../../install-Release/lib")
    endif()
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#-------------------------------------------------------------------------------
# Dependencies

# Base dependencies
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(minizip-ng REQUIRED)
find_package(OpenTimelineIO REQUIRED)

# OpenColorIO dependencies
if(TLRENDER_OCIO)
    find_package(OpenColorIO REQUIRED)
    add_definitions(-DTLRENDER_OCIO)
endif()

# SDL dependencies
if(TLRENDER_SDL2)
    find_package(SDL2 REQUIRED)
    add_definitions(-DTLRENDER_SDL2)
endif()
if(TLRENDER_SDL3)
    find_package(SDL3 REQUIRED)
    add_definitions(-DTLRENDER_SDL3)
endif()

# I/O dependencies
if(TLRENDER_JPEG)
    find_package(libjpeg-turbo)
    if(libjpeg-turbo_FOUND)
        add_definitions(-DTLRENDER_JPEG)
    endif()
endif()
if(TLRENDER_TIFF)
    find_package(TIFF)
    if(TIFF_FOUND)
        add_definitions(-DTLRENDER_TIFF)
    endif()
endif()
if(TLRENDER_PNG)
    find_package(PNG)
    if(PNG_FOUND)
        add_definitions(-DTLRENDER_PNG)
    endif()
endif()
if(TLRENDER_EXR)
    find_package(OpenEXR)
    if(OpenEXR_FOUND)
        add_definitions(-DTLRENDER_EXR)
    endif()
endif()
if(TLRENDER_FFMPEG)
    include(tlRenderFindFFmpeg)
    if(FFMPEG_FOUND)
        add_definitions(-DTLRENDER_FFMPEG)
        if(TLRENDER_FFMPEG_PLUGIN)
            add_definitions(-DTLRENDER_FFMPEG_PLUGIN)
        endif()
    endif()
endif()
if(TLRENDER_OIIO)
    find_package(OpenImageIO)
    if(OpenImageIO_FOUND)
        add_definitions(-DTLRENDER_OIIO)
    endif()
endif()
if(TLRENDER_USD)
    find_package(pxr)
    if(pxr_FOUND)
        add_definitions(-DTLRENDER_USD)
    endif()
endif()
if(TLRENDER_WMF)
    add_definitions(-DTLRENDER_WMF)
endif()
if(TLRENDER_AVF)
    add_definitions(-DTLRENDER_AVF)
endif()

# feather-tk dependencies
find_package(ftk REQUIRED)

# Qt dependencies
if(TLRENDER_QT6)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 REQUIRED COMPONENTS Quick Widgets OpenGLWidgets Svg HINTS "$ENV{QTDIR}")
endif()

# Python dependencies
if(TLRENDER_PYTHON)
    add_definitions(-DTLRENDER_PYTHON)
    find_package(Python3 REQUIRED Interpreter Development)
    find_package(pybind11 REQUIRED)
endif()

#-------------------------------------------------------------------------------
# Sub-directories

add_subdirectory(lib/tlRender)
if(TLRENDER_PROGRAMS)
    add_subdirectory(bin)
endif()
if(TLRENDER_TESTS)
    add_subdirectory(tests)
endif()
if(TLRENDER_EXAMPLES)
    add_subdirectory(examples)
endif()
add_subdirectory(etc/Legal)

include(CMakePackageConfigHelpers)
set(INCLUDE_INSTALL_DIR include/tlRender)
configure_package_config_file(
    tlRenderConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tlRenderConfig.cmake
    INSTALL_DESTINATION lib/cmake/tlRender
    PATH_VARS INCLUDE_INSTALL_DIR)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/tlRenderConfig.cmake
    DESTINATION lib/cmake/tlRender)

#-------------------------------------------------------------------------------
# Packaging

if(CMAKE_BUILD_TYPE MATCHES "^Release$")
    include(Package)
    include(CPack)
    include(InstallRequiredSystemLibraries)
endif()
