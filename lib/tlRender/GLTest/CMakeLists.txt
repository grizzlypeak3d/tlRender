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
if(TLRENDER_OCIO)
    # To ask the built-in configuration what displays and views it has,
    # rather than naming ones that may not be in every version of it.
    target_link_libraries(tlGLTest OpenColorIO::OpenColorIO)
endif()

target_compile_definitions(tlGLTest PUBLIC TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")

set_target_properties(tlGLTest PROPERTIES FOLDER tests)
