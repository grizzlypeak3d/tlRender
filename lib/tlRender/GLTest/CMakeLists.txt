set(HEADERS
    RenderTest.h)

set(SOURCE
    RenderTest.cpp)

add_library(tlGLTest ${SOURCE} ${HEADERS})

target_include_directories(tlGLTest
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlGLTest tlGL ftkTestLib)

set_target_properties(tlGLTest PROPERTIES FOLDER tests)
