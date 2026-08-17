# What continuous integration builds everywhere.
# The bindings are built here because nothing else builds them.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_PYTHON ON CACHE BOOL "")
set(ftk_PYTHON ON CACHE BOOL "")

# The command line FFmpeg plugin is in the platform configurations that do
# not run tests, not here: it registers ahead of the FFmpeg plugin and takes
# the movie reads, so a tested build would no longer be testing the library.
include("${CMAKE_CURRENT_LIST_DIR}/default.cmake")
