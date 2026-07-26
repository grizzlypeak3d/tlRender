set(HEADERS
    FrameToolBar.h
    Init.h
    ItemOptions.h
    PlaybackLoopWidget.h
    PlaybackToolBar.h
    ThumbnailSystem.h
    TimeEdit.h
    TimeLabel.h
    TimeUnitsWidget.h
    TimelineItem.h
    TimelineWidget.h
    Viewport.h)
set(HEADERS_PRIVATE
    TimelineItemPrivate.h)

set(SOURCE
    FrameToolBar.cpp
    Init.cpp
    ItemOptions.cpp
    PlaybackLoopWidget.cpp
    PlaybackToolBar.cpp
    ThumbnailSystem.cpp
    TimeEdit.cpp
    TimeLabel.cpp
    TimeUnitsWidget.cpp
    TimelineItem.cpp
    TimelineWidget.cpp
    Viewport.cpp)

add_library(tlUI ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})

target_include_directories(tlUI
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlUI PUBLIC tlGL ftk::ftkUI)

set_target_properties(tlUI PROPERTIES FOLDER lib)
set_target_properties(tlUI PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlUI
    EXPORT tlUITargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/UI)
install(
    EXPORT tlUITargets
    FILE tlUITargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::)
