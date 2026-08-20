# Continuous integration on Windows.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# The runner has no working OpenGL, so the tests that make a context are left
# out rather than the suite being skipped.
set(TLRENDER_TESTS_NO_GL ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
