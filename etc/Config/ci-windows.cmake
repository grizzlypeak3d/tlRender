# Continuous integration on Windows.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# The command line FFmpeg plugin, which is off by default and so is built
# nowhere else; DJV ships it. Only in the configurations that do not run
# tests, because it registers ahead of the FFmpeg plugin and takes the movie
# reads: a tested build would stop testing the library, and fail besides,
# since the runner has no ffmpeg for it to run.
set(TLRENDER_FFMPEG_CMD ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
