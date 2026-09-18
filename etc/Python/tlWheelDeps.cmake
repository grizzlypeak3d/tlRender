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

# Where the dependencies are built. Continuous integration keeps them
# between runs by naming a directory it caches; see wheels-workflow.yml.
if(DEFINED ENV{TLRENDER_WHEEL_DEPS})
    file(TO_CMAKE_PATH "$ENV{TLRENDER_WHEEL_DEPS}" TLRENDER_WHEEL_DEPS_DEFAULT)
else()
    set(TLRENDER_WHEEL_DEPS_DEFAULT "${CMAKE_BINARY_DIR}/deps")
endif()
set(TLRENDER_WHEEL_DEPS "${TLRENDER_WHEEL_DEPS_DEFAULT}" CACHE PATH "Dependencies prefix for wheel builds")

# The feather-tk wheel, a build requirement, is in pip's build environment,
# a directory named anew for every build. The dependencies record where the
# zlib and libpng they link are, so they are built against a copy of it at a
# path that stays put, beside them: a set built in an earlier run -- kept by
# continuous integration -- then still names something that is there.
execute_process(
    COMMAND ${Python_EXECUTABLE} -c
        "import importlib.util, os; print(os.path.dirname(importlib.util.find_spec('feather_tk').origin))"
    OUTPUT_VARIABLE TLRENDER_WHEEL_FTK_SOURCE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
file(TO_CMAKE_PATH "${TLRENDER_WHEEL_FTK_SOURCE}" TLRENDER_WHEEL_FTK_SOURCE)
set(TLRENDER_WHEEL_FTK "${TLRENDER_WHEEL_DEPS}-feather_tk")
file(REMOVE_RECURSE "${TLRENDER_WHEEL_FTK}")
file(COPY "${TLRENDER_WHEEL_FTK_SOURCE}/" DESTINATION "${TLRENDER_WHEEL_FTK}")
message(STATUS "Using feather-tk from ${TLRENDER_WHEEL_FTK_SOURCE}")

if(APPLE)
    # Homebrew's headers are in the compiler's own search path -- clang
    # reads /usr/local/include, where Homebrew installs on Intel -- and
    # CMAKE_IGNORE_PREFIX_PATH does not reach that: OpenImageIO compiled
    # against an Intel runner's newer libpng headers while linking the
    # 1.6.43 of the feather-tk wheel, and the link failed on png_get_cICP.
    # A directory given with -I is searched before the compiler's own, so
    # naming these first is what settles which headers are read. The
    # dependencies are handed the same flags below. Once only, since the
    # flags are cached and a build directory is configured again.
    set(TLRENDER_WHEEL_INCLUDES "-I${TLRENDER_WHEEL_FTK}/include -I${TLRENDER_WHEEL_DEPS}/include")
    foreach(flags CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
        string(FIND "${${flags}}" "${TLRENDER_WHEEL_INCLUDES}" found)
        if(found EQUAL -1)
            set(${flags} "${TLRENDER_WHEEL_INCLUDES} ${${flags}}" CACHE STRING "" FORCE)
        endif()
    endforeach()
endif()

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
set(TLRENDER_IGNORE_PREFIX_PATH \"${TLRENDER_IGNORE_PREFIX_PATH}\" CACHE STRING \"\")
")
    # What scikit-build-core decided for this build -- the compilers, the
    # Ninja it installed, and on macOS the architecture and deployment
    # target of the wheel -- has to hold for the dependencies as well.
    foreach(var
        CMAKE_C_COMPILER
        CMAKE_CXX_COMPILER
        CMAKE_C_FLAGS
        CMAKE_CXX_FLAGS
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
