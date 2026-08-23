set(HEADERS
    IOTest.h
    PNGTest.h
    RequestQueueTest.h)

set(SOURCE
    IOTest.cpp
    PNGTest.cpp
    RequestQueueTest.cpp)

if(TLRENDER_FFMPEG_PLUGIN)
    list(APPEND HEADERS FFmpegTest.h)
    list(APPEND SOURCE FFmpegTest.cpp)
endif()
if(TLRENDER_EXR)
    list(APPEND HEADERS EXRTest.h)
    list(APPEND SOURCE EXRTest.cpp)
endif()
if(TLRENDER_OIIO)
    list(APPEND HEADERS OIIOTest.h)
    list(APPEND SOURCE OIIOTest.cpp)
endif()
if(TLRENDER_SVG)
    list(APPEND HEADERS SVGTest.h)
    list(APPEND SOURCE SVGTest.cpp)
endif()

# Static, for the reason in ftk's TestLib.
add_library(tlIOTest STATIC ${SOURCE} ${HEADERS})
set_target_properties(tlIOTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tlIOTest
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlIOTest tlIO ftkTestLib)
if(TLRENDER_FFMPEG_PLUGIN)
    # The test asks FFmpeg itself what this build can encode, so it needs the
    # libraries rather than only the plugin that wraps them: tlIO links them
    # privately, and a shared library does not pass that on. A static build
    # never noticed, the symbols waiting for the program to be linked.
    target_link_libraries(tlIOTest FFmpeg::FFmpeg)
endif()

target_compile_definitions(tlIOTest PUBLIC TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")

set_target_properties(tlIOTest PROPERTIES FOLDER tests)
