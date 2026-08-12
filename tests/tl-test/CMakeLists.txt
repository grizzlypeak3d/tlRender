add_executable(tl-test tl-test.h tl-test.cpp)

target_include_directories(tl-test
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tl-test
    tlCoreTest
    tlGLTest
    tlIOTest
    tlTimelineTest
    tlUITest)

set_target_properties(tl-test PROPERTIES FOLDER tests)

add_test(
    NAME tl-test
    COMMAND tl-test -log)
