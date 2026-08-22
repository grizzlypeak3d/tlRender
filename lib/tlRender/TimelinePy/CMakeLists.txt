set(HEADERS
    Bindings.h)
set(HEADERS_PRIVATE)
set(SOURCE
    Audio.cpp
    AudioSystem.cpp
    BackgroundOptions.cpp
    Bindings.cpp
    ColorOptions.cpp
    CompareOptions.cpp
    DisplayOptions.cpp
    ForegroundOptions.cpp
    IRender.cpp
    Player.cpp
    PlayerOptions.cpp
    System.cpp
    TimeUnits.cpp
    Timeline.cpp
    TimelineOptions.cpp
    Transition.cpp
    Util.cpp
    Video.cpp)

add_library(tlTimelinePy ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlTimelinePy PRIVATE TL_EXPORTS)
endif()

target_include_directories(tlTimelinePy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlTimelinePy tlTimeline Python3::Python)

set_target_properties(tlTimelinePy PROPERTIES FOLDER lib)
set_target_properties(tlTimelinePy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlTimelinePy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/tlTimelinePy COMPONENT dev)
