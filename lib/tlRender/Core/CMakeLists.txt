set(HEADERS
    Audio.h
    AudioInline.h
    AudioResample.h
    AudioSystem.h
    Export.h
    HDR.h
    HDRInline.h
    Time.h
    TimeInline.h
    URL.h
    Util.h)

set(SOURCE
    Audio.cpp
    AudioResample.cpp
    AudioSystem.cpp
    HDR.cpp
    Time.cpp
    URL.cpp)

set(LIBRARIES tlResource ftk::ftkCore OTIO::opentimelineio Imath::Imath nlohmann_json::nlohmann_json)
set(LIBRARIES_PRIVATE Freetype::Freetype MINIZIP::minizip-ng ZLIB::ZLIB)
if(TLRENDER_OCIO)
    list(APPEND LIBRARIES OpenColorIO::OpenColorIO)
endif()
if(TLRENDER_SDL2)
    list(APPEND LIBRARIES_PRIVATE SDL2::SDL2)
endif()
if(TLRENDER_SDL3)
    list(APPEND LIBRARIES_PRIVATE SDL3::SDL3)
endif()
if(TLRENDER_FFMPEG)
    list(APPEND LIBRARIES_PRIVATE tlRender::FFmpeg)
endif()
list(APPEND LIBRARIES_PRIVATE Threads::Threads)

add_library(tlCore ${HEADERS} ${SOURCE})
target_link_libraries(tlCore PUBLIC ${LIBRARIES} PRIVATE ${LIBRARIES_PRIVATE})
set_target_properties(tlCore PROPERTIES FOLDER lib)
set_target_properties(tlCore PROPERTIES PUBLIC_HEADER "${HEADERS}")
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlCore PUBLIC TL_EXPORTS)
else()
    target_compile_definitions(tlCore PUBLIC TL_STATIC)
endif()

install(
    TARGETS tlCore
    EXPORT tlCoreTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/Core)
install(
    EXPORT tlCoreTargets
    FILE tlCoreTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::)
