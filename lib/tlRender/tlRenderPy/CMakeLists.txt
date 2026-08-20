pybind11_add_module(tlRenderPy Bindings.cpp)

target_include_directories(tlRenderPy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlRenderPy PRIVATE tlUIPy tlTimelinePy tlIOPy tlCorePy)

set_target_properties(tlRenderPy PROPERTIES FOLDER lib)

# The rpath convention of the executables names the interpreter's
# directory here, since the "executable" loading a Python module is
# Python itself: the module finds the libraries beside it instead.
if(APPLE)
    set_target_properties(tlRenderPy PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(tlRenderPy PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

set_target_properties(tlRenderPy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlRenderPy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
