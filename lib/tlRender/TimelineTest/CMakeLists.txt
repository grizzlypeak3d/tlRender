set(HEADERS
    AudioSystemTest.h
    BackgroundOptionsTest.h
    CPUColorTest.h
    ColorOptionsTest.h
    CompareOptionsTest.h
    DisplayOptionsTest.h
    ForegroundOptionsTest.h
    PlayerOptionsTest.h
    PlayerAudioTest.h
    PlayerTest.h
    TimeUnitsTest.h
    TimelineTest.h
    UtilTest.h)

set(SOURCE
    AudioSystemTest.cpp
    BackgroundOptionsTest.cpp
    CPUColorTest.cpp
    ColorOptionsTest.cpp
    CompareOptionsTest.cpp
    DisplayOptionsTest.cpp
    ForegroundOptionsTest.cpp
    PlayerOptionsTest.cpp
    PlayerAudioTest.cpp
    PlayerTest.cpp
    TimeUnitsTest.cpp
    TimelineTest.cpp
    UtilTest.cpp)

# Static, for the reason in ftk's TestLib.
add_library(tlTimelineTest STATIC ${SOURCE} ${HEADERS})
set_target_properties(tlTimelineTest PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(tlTimelineTest PUBLIC TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")

target_include_directories(tlTimelineTest
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlTimelineTest tlTimeline ftk::ftkTestLib)
if(TLRENDER_OCIO)
    # To ask the built-in configuration what displays and views it has,
    # rather than naming ones that may not be in every version of it.
    target_link_libraries(tlTimelineTest OpenColorIO::OpenColorIO)
endif()

set_target_properties(tlTimelineTest PROPERTIES FOLDER tests)
