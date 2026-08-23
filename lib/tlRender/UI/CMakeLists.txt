set(HEADERS
    FileBrowserThumbnails.h
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
    TimelineRuler.h
    TimelineWidget.h
    Viewport.h)
set(HEADERS_PRIVATE
    TimelineItemPrivate.h)

set(SOURCE
    FileBrowserThumbnails.cpp
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
    TimelineRuler.cpp
    TimelineWidget.cpp
    Viewport.cpp)

add_library(tlUI ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})
# Private when building, so this library's own API is dllexport here and
# dllimport to everything else. Public when static, which a consumer does
# have to know: the API then carries no declspec at all.
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlUI PRIVATE TL_UI_EXPORTS)
else()
    target_compile_definitions(tlUI PUBLIC TL_UI_STATIC)
endif()

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
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/UI COMPONENT dev)
install(
    EXPORT tlUITargets
    FILE tlUITargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
