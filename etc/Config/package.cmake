# A package build: FFmpeg cut down to the codecs that can be shipped without
# a license, with both ways of reaching it, and no tests or examples. The
# plugin against that minimal build is enough for limited exports; the command
# line tool is how someone brings their own codecs.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_FFMPEG_MINIMAL ON CACHE BOOL "")
set(TLRENDER_FFMPEG_CMD ON CACHE BOOL "")
set(TLRENDER_SUBPROCESS ON CACHE BOOL "")
set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "")
set(TLRENDER_EXAMPLES OFF CACHE BOOL "")
set(TLRENDER_TESTS OFF CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
