# What continuous integration builds everywhere.
# The bindings are built here because nothing else builds them.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_PYTHON ON CACHE BOOL "")
set(ftk_PYTHON ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/default.cmake")
