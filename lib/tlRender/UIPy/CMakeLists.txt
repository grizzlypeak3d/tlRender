set(HEADERS
    Bindings.h
    ItemOptions.h
    PlaybackLoopWidget.h
    ThumbnailSystem.h
    TimeEdit.h
    TimeLabel.h
    TimeUnitsWidget.h
    TimelineRuler.h
    TimelineWidget.h
    Viewport.h)
set(HEADERS_PRIVATE)
set(SOURCE
    Bindings.cpp
    FrameToolBar.cpp
    ItemOptions.cpp
    PlaybackLoopWidget.cpp
    PlaybackToolBar.cpp
    ThumbnailSystem.cpp
    TimeEdit.cpp
    TimeLabel.cpp
    TimeUnitsWidget.cpp
    TimelineRuler.cpp
    TimelineWidget.cpp
    Viewport.cpp)

# Static. The bindings are linked by tlRenderPy and nothing else, so there
# is no API here for anything to import -- the module absorbs them.
add_library(tlUIPy STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})
# Hidden, as the module is: the binding classes hold nanobind's types,
# which are hidden, and GCC warns about a class more visible than its
# members. Nothing here is exported anyway.
set_target_properties(tlUIPy PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_include_directories(tlUIPy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)
target_include_directories(tlUIPy
    PRIVATE
        ${NB_DIR}/include
        ${NB_DIR}/ext/robin_map/include)

target_link_libraries(tlUIPy tlUI Python3::Python)

set_target_properties(tlUIPy PROPERTIES FOLDER lib)
set_target_properties(tlUIPy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlUIPy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/tlUIPy COMPONENT dev)
