pybind11_add_module(tlRenderPy Bindings.cpp)

target_include_directories(tlRenderPy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlRenderPy PRIVATE tlUIPy tlTimelinePy tlIOPy tlCorePy)

set_target_properties(tlRenderPy PROPERTIES FOLDER lib)
set_target_properties(tlRenderPy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlRenderPy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
