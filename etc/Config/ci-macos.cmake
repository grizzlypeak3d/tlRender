# Continuous integration on macOS.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# Newer than DJV's package, which still goes back to 10.15.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14" CACHE STRING "")

# The command line FFmpeg plugin, which is off by default and so is built
# nowhere else; DJV ships it. Only in the configurations that do not run
# tests, because it registers ahead of the FFmpeg plugin and takes the movie
# reads: a tested build would stop testing the library, and fail besides,
# since the runner has no ffmpeg for it to run.

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
