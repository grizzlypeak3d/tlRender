# The C interface, from C. The bundles are written under the build
# directory, cleared before each run by the fixture: a bundle is never
# written over one already there.
add_executable(tl-c-test tl-c-test.c)
target_link_libraries(tl-c-test tlC)
target_compile_definitions(tl-c-test PRIVATE
    TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData"
    TL_C_TEST_OUT="${CMAKE_CURRENT_BINARY_DIR}/out")
set_target_properties(tl-c-test PROPERTIES
    FOLDER tests
    C_STANDARD 11
    C_STANDARD_REQUIRED ON)

add_test(
    NAME tl-c-test-out
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/out)
add_test(
    NAME tl-c-test-mkdir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/out)
set_tests_properties(tl-c-test-out PROPERTIES FIXTURES_SETUP tl-c-test-clean)
set_tests_properties(tl-c-test-mkdir PROPERTIES
    FIXTURES_SETUP tl-c-test-clean
    DEPENDS tl-c-test-out)
add_test(NAME tl-c-test COMMAND tl-c-test)
# No sound card on a runner, and no screen: SDL's dummy drivers, and the
# GPU renderer is left out when it cannot start, which the test allows.
# OpenTimelineIO's libraries are on PATH for the reason in tl-test.
set_tests_properties(tl-c-test PROPERTIES
    FIXTURES_REQUIRED tl-c-test-clean
    ENVIRONMENT "SDL_AUDIO_DRIVER=dummy"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_INSTALL_PREFIX}/python/opentimelineio")
