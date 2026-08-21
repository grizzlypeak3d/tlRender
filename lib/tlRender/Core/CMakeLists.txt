set(HEADERS
    Audio.h
    AudioInline.h
    AudioResample.h
    Export.h
    HDR.h
    HDRInline.h
    Time.h
    TimeInline.h
    URL.h
    Util.h
    Version.h)

set(SOURCE
    Audio.cpp
    AudioResample.cpp
    HDR.cpp
    Time.cpp
    URL.cpp)

add_library(tlCore ${HEADERS} ${SOURCE})

if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlCore PUBLIC TL_EXPORTS)
else()
    target_compile_definitions(tlCore PUBLIC TL_STATIC)
endif()
target_compile_definitions(tlCore PUBLIC $<$<CONFIG:Debug>:FTK_ASSERT>)

target_include_directories(tlCore
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlCore
    PUBLIC tlResource ftk::ftkCore OTIO::opentimelineio Imath::Imath)

if(TLRENDER_OCIO)
    # Public because this is not the only library that calls OpenColorIO:
    # tlTimeline names it for the LUT formats, tlGL for the shaders, and DJV's
    # models beyond that. A private link reaches them anyway while everything
    # is static -- a static OpenColorIO is folded into this library and
    # re-exported -- and stops reaching them the moment either becomes shared.
    target_compile_definitions(tlCore PUBLIC TLRENDER_OCIO)
    target_link_libraries(tlCore PUBLIC OpenColorIO::OpenColorIO)
endif()
if(TLRENDER_FFMPEG)
    target_compile_definitions(tlCore PUBLIC TLRENDER_FFMPEG)
    target_link_libraries(tlCore PRIVATE FFmpeg::swresample FFmpeg::avutil)
endif()

set_target_properties(tlCore PROPERTIES FOLDER lib)
set_target_properties(tlCore PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlCore
    EXPORT tlCoreTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/Core COMPONENT dev)
install(
    EXPORT tlCoreTargets
    FILE tlCoreTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
