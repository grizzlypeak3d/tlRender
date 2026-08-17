# What continuous integration builds everywhere.
# The bindings are built here because nothing else builds them.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_PYTHON ON CACHE BOOL "")
set(ftk_PYTHON ON CACHE BOOL "")

# The command line FFmpeg plugin, which is off by default and so is built
# nowhere else. DJV ships it, and until the package configurations were
# removed from here it was compiled only by those. It is built rather than
# tested: running it needs an ffmpeg on the machine.
set(TLRENDER_FFMPEG_CMD ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/default.cmake")
