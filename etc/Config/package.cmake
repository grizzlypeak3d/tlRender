# A package build: FFmpeg cut down to the parts that are shipped, the
# command line tool instead of the plugin, and no tests or examples.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_FFMPEG_MINIMAL ON CACHE BOOL "")
set(TLRENDER_FFMPEG_CMD ON CACHE BOOL "")
set(TLRENDER_SUBPROCESS ON CACHE BOOL "")
set(TLRENDER_FFMPEG_PLUGIN OFF CACHE BOOL "")
set(TLRENDER_EXAMPLES OFF CACHE BOOL "")
set(TLRENDER_TESTS OFF CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
