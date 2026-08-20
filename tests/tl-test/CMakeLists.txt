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

# The macOS and Windows CI runners have no working OpenGL. Leaving out the
# tests that make a context is better than skipping the suite, which is what
# those platforms did before.
if(TLRENDER_TESTS_NO_GL)
    add_test(
        NAME tl-test
        COMMAND tl-test -log -noGL)
else()
    add_test(
        NAME tl-test
        COMMAND tl-test -log)
endif()

# A test that hangs should fail, not run until the job is killed: this one
# waits on futures, and a reader that never answers waits forever. The whole
# suite is a couple of minutes, so this only fires on a hang.
set_tests_properties(tl-test PROPERTIES TIMEOUT 900)
