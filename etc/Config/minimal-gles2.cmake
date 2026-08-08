# The minimal build against OpenGL ES 2. OpenEXR stays in.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(ftk_API "GLES_2" CACHE STRING "")
set(TLRENDER_EXR ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/minimal.cmake")
