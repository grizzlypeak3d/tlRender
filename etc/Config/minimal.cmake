# The smallest useful build: none of the media libraries, so a change
# that depends on one of them fails here rather than in a package.
include("${CMAKE_CURRENT_LIST_DIR}/local.cmake" OPTIONAL)

set(TLRENDER_OCIO OFF CACHE BOOL "")
set(TLRENDER_JPEG OFF CACHE BOOL "")
set(TLRENDER_TIFF OFF CACHE BOOL "")
set(TLRENDER_EXR OFF CACHE BOOL "")
set(TLRENDER_AOM OFF CACHE BOOL "")
set(TLRENDER_SVTAV1 OFF CACHE BOOL "")
set(TLRENDER_FFMPEG OFF CACHE BOOL "")
set(TLRENDER_FFMPEG_PLUGIN OFF CACHE BOOL "")
set(TLRENDER_NASM OFF CACHE BOOL "")
set(TLRENDER_OIIO OFF CACHE BOOL "")
set(TLRENDER_PYTHON OFF CACHE BOOL "")
set(ftk_PYTHON OFF CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/ci.cmake")
