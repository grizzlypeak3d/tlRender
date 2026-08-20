# Continuous integration on macOS.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# Newer than DJV's package, which still goes back to 10.15.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14" CACHE STRING "")

# The runner has no working OpenGL, so the tests that make a context are left
# out rather than the suite being skipped.
set(TLRENDER_TESTS_NO_GL ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
