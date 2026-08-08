# Continuous integration on Linux, which also has Qt6 and coverage.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_QT6 ON CACHE BOOL "")
set(TLRENDER_GCOV ON CACHE BOOL "")
set(TLRENDER_FFMPEG_PLUGIN ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
