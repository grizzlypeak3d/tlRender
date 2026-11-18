set(HEADERS
    GestureTest.h
    ThumbnailColorsTest.h
    ThumbnailSystemTest.h)

set(SOURCE
    GestureTest.cpp
    ThumbnailColorsTest.cpp
    ThumbnailSystemTest.cpp)

# Static, for the reason in ftk's TestLib.
add_library(tlUITest STATIC ${SOURCE} ${HEADERS})
set_target_properties(tlUITest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(tlUITest PUBLIC TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")

target_include_directories(tlUITest
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlUITest tlUI ftk::ftkTestLib)
if(TLRENDER_OCIO)
    # To ask the built-in configuration what displays and views it has.
    target_link_libraries(tlUITest OpenColorIO::OpenColorIO)
endif()

set_target_properties(tlUITest PROPERTIES FOLDER tests)
