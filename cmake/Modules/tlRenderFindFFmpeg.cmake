if(TARGET tlRender::FFmpeg)
    return()
endif()

if(NOT TLRENDER_FFMPEG AND NOT TLRENDER_FFMPEG_PLUGIN)
    return()
endif()

if(TLRENDER_FFMPEG_PLUGIN)
    find_package(FFMPEG REQUIRED COMPONENTS
        avcodec avdevice avfilter avformat avutil swresample swscale)
else()
    find_package(FFMPEG REQUIRED COMPONENTS avutil swresample)
endif()

add_library(tlRender::FFmpeg INTERFACE IMPORTED)
set_target_properties(tlRender::FFmpeg PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}")
target_link_libraries(tlRender::FFmpeg INTERFACE
    ${FFMPEG_LIBRARIES})

message(STATUS "tlRender::FFmpeg: ${FFMPEG_LIBRARIES}")
