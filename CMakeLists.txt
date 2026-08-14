cmake_minimum_required(VERSION 3.31)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0114 NEW)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

file(READ "lib/tlRender/Core/Version.h" VERSION_H)
string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${VERSION_H})
set(TLRENDER_VERSION_MAJOR ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${VERSION_H})
set(TLRENDER_VERSION_MINOR ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${VERSION_H})
set(TLRENDER_VERSION_PATCH ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_DEV \"([0-9a-z\.\-]*)\"" _ ${VERSION_H})
set(TLRENDER_VERSION_DEV ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_FULL \"([0-9a-z\.\-]*)\"" _ ${VERSION_H})
set(TLRENDER_VERSION_FULL ${CMAKE_MATCH_1})

project(
    tlRender
    VERSION ${TLRENDER_VERSION_MAJOR}.${TLRENDER_VERSION_MINOR}.${TLRENDER_VERSION_PATCH}
    DESCRIPTION "tlRender is an open source library for building playback and review applications for visual effects, film, and animation"
    HOMEPAGE_URL "https://github.com/grizzlypeak3d/tlRender"
    LANGUAGES CXX C)

#-------------------------------------------------------------------------------
# Build options

set(TLRENDER_OCIO ON CACHE BOOL "Enable support for OpenColorIO")
set(TLRENDER_EXR ON CACHE BOOL "Enable support for EXR I/O")
set(TLRENDER_FFMPEG ON CACHE BOOL "Enable support for FFmpeg (required for audio)")
set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "Enable support for FFmpeg I/O")
set(TLRENDER_FFMPEG_CMD OFF CACHE BOOL "Enable support for FFmpeg I/O with the command line application")
set(TLRENDER_OIIO ON CACHE BOOL "Enable support for OpenImageIO")
set(TLRENDER_SVG ON CACHE BOOL "Enable support for SVG I/O")
set(TLRENDER_USD OFF CACHE BOOL "Enable support for USD")
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
set(TLRENDER_SANITIZE "" CACHE STRING "Enable sanitizers (comma-separated, e.g. address,undefined)")

#-------------------------------------------------------------------------------
# Internal configuration

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

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

if(TLRENDER_TESTS)
    set(CTEST_OUTPUT_ON_FAILURE ON)
    enable_testing()
endif()

if(TLRENDER_GPROF)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif()
if(TLRENDER_SANITIZE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${TLRENDER_SANITIZE} -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${TLRENDER_SANITIZE} -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${TLRENDER_SANITIZE}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${TLRENDER_SANITIZE}")
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
    set(CMAKE_BUILD_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#-------------------------------------------------------------------------------
# Dependencies

# Base dependencies
find_package(ZLIB REQUIRED)
find_package(Imath REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(minizip-ng REQUIRED)
find_package(OpenTimelineIO REQUIRED)

# OpenColorIO dependencies
if(TLRENDER_OCIO)
    find_package(OpenColorIO REQUIRED)
endif()

# I/O dependencies
if(TLRENDER_EXR)
    find_package(OpenEXR REQUIRED)
endif()
if(TLRENDER_FFMPEG)
    if(WIN32)
        set(FFmpeg_FOUND TRUE)
        add_library(FFmpeg::avutil STATIC IMPORTED)
        set_property(TARGET FFmpeg::avutil PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/avutil.lib")
        add_library(FFmpeg::avdevice STATIC IMPORTED)
        set_property(TARGET FFmpeg::avdevice PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/avdevice.lib")
        add_library(FFmpeg::avformat STATIC IMPORTED)
        set_property(TARGET FFmpeg::avformat PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/avformat.lib")
        add_library(FFmpeg::avcodec STATIC IMPORTED)
        set_property(TARGET FFmpeg::avcodec PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/avcodec.lib")
        add_library(FFmpeg::swresample STATIC IMPORTED)
        set_property(TARGET FFmpeg::swresample PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/swresample.lib")
        add_library(FFmpeg::swscale STATIC IMPORTED)
        set_property(TARGET FFmpeg::swscale PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/swscale.lib")
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        set_property(TARGET FFmpeg::FFmpeg PROPERTY
            INTERFACE_LINK_LIBRARIES
            FFmpeg::avdevice
            FFmpeg::avformat
            FFmpeg::avcodec
            FFmpeg::swresample
            FFmpeg::swscale
            FFmpeg::avutil)
    elseif(APPLE)
        set(FFmpeg_FOUND TRUE)
        add_library(FFmpeg::avdevice STATIC IMPORTED)
        set_property(TARGET FFmpeg::avdevice PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libavdevice.dylib")
        add_library(FFmpeg::avformat STATIC IMPORTED)
        set_property(TARGET FFmpeg::avformat PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libavformat.dylib")
        add_library(FFmpeg::avcodec STATIC IMPORTED)
        set_property(TARGET FFmpeg::avcodec PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libavcodec.dylib")
        add_library(FFmpeg::swresample STATIC IMPORTED)
        set_property(TARGET FFmpeg::swresample PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libswresample.dylib")
        add_library(FFmpeg::swscale STATIC IMPORTED)
        set_property(TARGET FFmpeg::swscale PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libswscale.dylib")
        add_library(FFmpeg::avutil STATIC IMPORTED)
        set_property(TARGET FFmpeg::avutil PROPERTY
             IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libavutil.dylib")
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        set_property(TARGET FFmpeg::FFmpeg PROPERTY
            INTERFACE_LINK_LIBRARIES
            FFmpeg::avdevice
            FFmpeg::avformat
            FFmpeg::avcodec
            FFmpeg::swresample
            FFmpeg::swscale
            FFmpeg::avutil)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(
            swresample REQUIRED IMPORTED_TARGET
            libswresample)
        pkg_check_modules(
            avutil REQUIRED IMPORTED_TARGET
            libavutil)
        pkg_check_modules(
            FFmpeg REQUIRED IMPORTED_TARGET
            libavdevice
            libavformat
            libavcodec
            libswresample
            libswscale
            libavutil)
        add_library(FFmpeg::FFmpeg ALIAS PkgConfig::FFmpeg)
        add_library(FFmpeg::swresample ALIAS PkgConfig::swresample)
        add_library(FFmpeg::avutil ALIAS PkgConfig::avutil)
    endif()
endif()
if(TLRENDER_OIIO)
    find_package(OpenImageIO)
endif()
if(TLRENDER_SVG)
    # Also a dependency of ftk, which is where it comes from.
    find_package(lunasvg REQUIRED)
endif()
if(TLRENDER_USD)
    find_package(OpenGL REQUIRED)
    find_package(pxr REQUIRED)
endif()

# Python dependencies
if(TLRENDER_PYTHON)
    find_package(Python3 REQUIRED Interpreter Development)
    find_package(pybind11 REQUIRED)
endif()

#-------------------------------------------------------------------------------
# Git submodules

find_package(Git REQUIRED)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/ftk/CMakeLists.txt")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMODULE_RESULT)
    if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
        message(FATAL_ERROR "git submodule update failed with ${GIT_SUBMODULE_RESULT}")
    endif()
endif()

#-------------------------------------------------------------------------------
# Sub-directories

add_subdirectory(deps/ftk)
add_library(ftk::ftkResource ALIAS ftkResource)
add_library(ftk::ftkCore ALIAS ftkCore)
add_library(ftk::ftkGL ALIAS ftkGL)
add_library(ftk::ftkUI ALIAS ftkUI)
add_library(ftk::ftkTestLib ALIAS ftkTestLib)

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
