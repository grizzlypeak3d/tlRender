set(HEADERS
    Bindings.h)
set(HEADERS_PRIVATE)
set(SOURCE
    Audio.cpp
    AudioResample.cpp
    Bindings.cpp
    HDR.cpp
    Time.cpp
    URL.cpp)

add_library(tlCorePy ${HEADERS} ${HEADERS_PRIVATE} ${SOURCE})
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlCorePy PRIVATE TL_EXPORTS)
endif()

target_include_directories(tlCorePy
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlCorePy tlCore Python3::Python)

set_target_properties(tlCorePy PROPERTIES FOLDER lib)
set_target_properties(tlCorePy PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlCorePy
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/tlCorePy COMPONENT dev)
