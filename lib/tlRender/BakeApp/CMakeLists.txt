set(HEADERS
    App.h)

set(SOURCE
    App.cpp)

add_library(tlBakeApp ${HEADERS} ${SOURCE})

target_include_directories(tlBakeApp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlBakeApp tlGL)

set_target_properties(tlBakeApp PROPERTIES FOLDER lib)
set_target_properties(tlBakeApp PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlBakeApp
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/BakeApp COMPONENT dev)
