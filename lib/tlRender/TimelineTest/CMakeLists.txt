set(HEADERS
    AudioSystemTest.h
    BackgroundOptionsTest.h
    ColorOptionsTest.h
    CompareOptionsTest.h
    DisplayOptionsTest.h
    ForegroundOptionsTest.h
    PlayerOptionsTest.h
    PlayerTest.h
    TimeUnitsTest.h
    TimelineTest.h
    UtilTest.h)

set(SOURCE
    AudioSystemTest.cpp
    BackgroundOptionsTest.cpp
    ColorOptionsTest.cpp
    CompareOptionsTest.cpp
    DisplayOptionsTest.cpp
    ForegroundOptionsTest.cpp
    PlayerOptionsTest.cpp
    PlayerTest.cpp
    TimeUnitsTest.cpp
    TimelineTest.cpp
    UtilTest.cpp)

add_library(tlTimelineTest ${SOURCE} ${HEADERS})

target_compile_definitions(tlTimelineTest PUBLIC TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")

target_include_directories(tlTimelineTest
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlTimelineTest tlTimeline ftkTestLib)

set_target_properties(tlTimelineTest PROPERTIES FOLDER tests)
