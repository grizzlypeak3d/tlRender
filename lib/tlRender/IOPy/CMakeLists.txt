set(HEADERS
    Bindings.h)
set(HEADERS_PRIVATE)
set(SOURCE
    Bindings.cpp
    IO.cpp
    Plugin.cpp
    Read.cpp
    SeqIO.cpp
    System.cpp
    Write.cpp)

# Static. The bindings are linked by tlRenderPy and nothing else, so there
# is no API here for anything to import -- the module absorbs them.
add_library(tlIOPy STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})
set_target_properties(tlIOPy PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tlIOPy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlIOPy tlIO Python3::Python)

set_target_properties(tlIOPy PROPERTIES FOLDER lib)
set_target_properties(tlIOPy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlIOPy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/tlIOPy COMPONENT dev)
