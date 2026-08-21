set(HEADERS
    Bindings.h)
set(HEADERS_PRIVATE)
set(SOURCE
    Bindings.cpp
    IO.cpp
    Plugin.cpp
    Read.cpp
    System.cpp
    Write.cpp)

add_library(tlIOPy ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})

target_include_directories(tlIOPy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlIOPy tlIO pybind11::headers)

set_target_properties(tlIOPy PROPERTIES FOLDER lib)
set_target_properties(tlIOPy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlIOPy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/tlIOPy COMPONENT dev)
