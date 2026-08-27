# The minimal build against OpenGL ES 3. OpenEXR stays in.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(ftk_API "GLES_3" CACHE STRING "")
set(TLRENDER_EXR ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/minimal.cmake")
