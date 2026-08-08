# A Linux package, which also carries Qt6.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_QT6 ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/package.cmake")
