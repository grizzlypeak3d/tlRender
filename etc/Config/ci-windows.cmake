# Continuous integration on Windows.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

# The runner has no working OpenGL, so the tests that make a context are left
# out rather than the suite being skipped. feather-tk is built here as part of
# the superbuild and reads this file rather than its own, so it has to be told
# as well -- and until it was, its tests and examples ran and crashed.
set(TLRENDER_TESTS_NO_GL ON CACHE BOOL "")
set(ftk_TESTS_NO_GL ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
