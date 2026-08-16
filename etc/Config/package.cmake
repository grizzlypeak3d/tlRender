# A package build: FFmpeg cut down to the codecs that can be shipped without
# a license, with both ways of reaching it, and no tests or examples. The
# plugin against that minimal build is enough for limited exports; the command
# line tool is how someone brings their own codecs.
# A package build takes no personal settings. local.cmake is where the tests,
# examples, programs and Python bindings are turned on, and none of those
# belong in what someone installs.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/local.cmake")
    message(FATAL_ERROR
        "etc/Config/local.cmake is present; move it aside for a package build")
endif()

# FORCE, so that nothing included below can change what a package ships. A -D
# on the command line still wins, since CMake reads those after this file.
set(TLRENDER_FFMPEG_MINIMAL ON CACHE BOOL "" FORCE)
set(TLRENDER_FFMPEG_CMD ON CACHE BOOL "" FORCE)
set(TLRENDER_SUBPROCESS ON CACHE BOOL "" FORCE)
set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "" FORCE)
set(TLRENDER_EXAMPLES OFF CACHE BOOL "")
set(TLRENDER_TESTS OFF CACHE BOOL "")

# The defaults, not ci.cmake: continuous integration builds the Python
# bindings because nothing else does, which is a reason to build them for
# testing and not a reason to ship them.
include("${CMAKE_CURRENT_LIST_DIR}/default.cmake")
