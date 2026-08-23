set(HEADERS
    Audio.h
    AudioSystem.h
    BackgroundOptions.h
    ColorOptions.h
    CompareOptions.h
    DisplayOptions.h
    ForegroundOptions.h
    IRender.h
    Init.h
    Player.h
    PlayerOptions.h
    System.h
    TimeUnits.h
    Timeline.h
    TimelineOptions.h
    Transition.h
    Util.h
    UtilInline.h
    Video.h)
set(PRIVATE_HEADERS
    PlayerPrivate.h
    TimelinePrivate.h
    ZipPrivate.h)

set(SOURCE
    Audio.cpp
    AudioSystem.cpp
    BackgroundOptions.cpp
    ColorOptions.cpp
    CompareOptions.cpp
    DisplayOptions.cpp
    ForegroundOptions.cpp
    IRender.cpp
    Init.cpp
    Player.cpp
    PlayerAudio.cpp
    PlayerOptions.cpp
    PlayerPrivate.cpp
    System.cpp
    TimeUnits.cpp
    Timeline.cpp
    TimelineOptions.cpp
    Transition.cpp
    Util.cpp
    Video.cpp
    Zip.cpp)

add_library(tlTimeline ${HEADERS} ${PRIVATE_HEADERS} ${SOURCE})
# Private when building, so this library's own API is dllexport here and
# dllimport to everything else. Public when static, which a consumer does
# have to know: the API then carries no declspec at all.
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlTimeline PRIVATE TL_TIMELINE_EXPORTS)
else()
    target_compile_definitions(tlTimeline PUBLIC TL_TIMELINE_STATIC)
endif()

target_include_directories(tlTimeline
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlTimeline tlIO MINIZIP::minizip-ng)

set_target_properties(tlTimeline PROPERTIES FOLDER lib)
set_target_properties(tlTimeline PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlTimeline
    EXPORT tlTimelineTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/Timeline COMPONENT dev)
install(
    EXPORT tlTimelineTargets
    FILE tlTimelineTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
