# The dependencies of a wheel build. pip runs one CMake project and nothing
# before it, so the super build is run from here, at configure time, into the
# binary directory; everything it installs goes into the wheel with tlRender.
#
# feather-tk and what it brings -- zlib, libpng, FreeType, SDL -- are not
# built: they come from the feather-tk wheel, a build requirement, and the
# dependencies built here link those copies. Carrying a second libz beside
# feather-tk's would not even work on Linux, where the loader takes whichever
# library of a name it loaded first.
#
# A prefix that is already complete is used as it is: keep scikit-build-core's
# build directory (build-dir in pyproject.toml) to build them once, or point
# TLRENDER_WHEEL_DEPS at a prefix built some other way.

# Where the feather-tk wheel is, without importing it.
execute_process(
    COMMAND ${Python_EXECUTABLE} -c
        "import importlib.util, os; print(os.path.dirname(importlib.util.find_spec('feather_tk').origin))"
    OUTPUT_VARIABLE TLRENDER_WHEEL_FTK
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
file(TO_CMAKE_PATH "${TLRENDER_WHEEL_FTK}" TLRENDER_WHEEL_FTK)
message(STATUS "Using feather-tk from ${TLRENDER_WHEEL_FTK}")

set(TLRENDER_WHEEL_DEPS "${CMAKE_BINARY_DIR}/deps" CACHE PATH "Dependencies prefix for wheel builds")
set(TLRENDER_WHEEL_DEPS_STAMP "${TLRENDER_WHEEL_DEPS}/.tlrender-wheel-deps")

if(NOT EXISTS "${TLRENDER_WHEEL_DEPS_STAMP}")
    set(config ${CMAKE_BUILD_TYPE})
    if(NOT config)
        set(config Release)
    endif()

    # An initial cache rather than -D arguments, since the prefix path is a
    # list and a command line would split it.
    set(init "${CMAKE_BINARY_DIR}/deps-init.cmake")
    file(WRITE ${init} "
set(CMAKE_BUILD_TYPE \"${config}\" CACHE STRING \"\")
set(CMAKE_INSTALL_PREFIX \"${TLRENDER_WHEEL_DEPS}\" CACHE PATH \"\")
set(CMAKE_PREFIX_PATH \"${TLRENDER_WHEEL_DEPS};${TLRENDER_WHEEL_FTK}\" CACHE STRING \"\")
set(BUILD_SHARED_LIBS ON CACHE BOOL \"\")
set(TLRENDER_USD OFF CACHE BOOL \"\")
set(TLRENDER_PYTHON OFF CACHE BOOL \"\")
set(TLRENDER_FFMPEG_MINIMAL \"${TLRENDER_FFMPEG_MINIMAL}\" CACHE BOOL \"\")
")
    # What scikit-build-core decided for this build -- the compilers, the
    # Ninja it installed, and on macOS the architecture and deployment
    # target of the wheel -- has to hold for the dependencies as well.
    foreach(var
        CMAKE_C_COMPILER
        CMAKE_CXX_COMPILER
        CMAKE_MAKE_PROGRAM
        CMAKE_OSX_ARCHITECTURES
        CMAKE_OSX_DEPLOYMENT_TARGET)
        if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
            file(APPEND ${init} "set(${var} \"${${var}}\" CACHE STRING \"\")\n")
        endif()
    endforeach()

    set(args
        -S ${PROJECT_SOURCE_DIR}/etc/SuperBuild
        -B ${CMAKE_BINARY_DIR}/deps-build
        -G ${CMAKE_GENERATOR}
        -C ${init})
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND args -A ${CMAKE_GENERATOR_PLATFORM})
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND args -T ${CMAKE_GENERATOR_TOOLSET})
    endif()

    message(STATUS "Building the wheel dependencies into ${TLRENDER_WHEEL_DEPS}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} ${args}
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/deps-build --config ${config}
        COMMAND_ERROR_IS_FATAL ANY)

    # Each library finds the others beside it and feather-tk's in the
    # feather_tk package, whatever its own build gave it.
    execute_process(
        COMMAND ${Python_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/fix_rpaths.py
            ${TLRENDER_WHEEL_DEPS}
            ${TLRENDER_INSTALL_RPATH_EXTRA}
        COMMAND_ERROR_IS_FATAL ANY)
    file(TOUCH "${TLRENDER_WHEEL_DEPS_STAMP}")
endif()

list(PREPEND CMAKE_PREFIX_PATH ${TLRENDER_WHEEL_DEPS} ${TLRENDER_WHEEL_FTK})
set(TLRENDER_FFMPEG_PREFIX "${TLRENDER_WHEEL_DEPS}" CACHE PATH "" FORCE)
# FFmpeg is found with pkg-config on Linux.
if(UNIX AND NOT APPLE)
    set(ENV{PKG_CONFIG_PATH} "${TLRENDER_WHEEL_DEPS}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# Everything but what only means something on this machine or at build
# time: pkg-config files hold its absolute paths, and the programs are the
# assembler and the like.
install(
    DIRECTORY ${TLRENDER_WHEEL_DEPS}/
    DESTINATION .
    USE_SOURCE_PERMISSIONS
    PATTERN .tlrender-wheel-deps EXCLUDE
    PATTERN pkgconfig EXCLUDE
    PATTERN "*.a" EXCLUDE
    PATTERN "*.exe" EXCLUDE
    PATTERN nasm EXCLUDE
    PATTERN ndisasm EXCLUDE
    REGEX "/share/(doc|man)$" EXCLUDE)
