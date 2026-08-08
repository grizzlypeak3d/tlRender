# Continuous integration on macOS.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "")

# Newer than the package, which still goes back to 10.15.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14" CACHE STRING "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
