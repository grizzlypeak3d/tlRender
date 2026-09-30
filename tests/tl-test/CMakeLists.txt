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

# OpenTimelineIO's libraries are not in bin. Building its Python package puts
# them beside the module instead, in python/opentimelineio, which is what the
# package needs and is nowhere the loader looks. This links opentimelineio, so
# without the directory on PATH the program does not start, and on Windows a
# missing library is a message box rather than an error -- on a continuous
# integration runner that is a test that never finishes.
#
# On PATH rather than copied into bin: two images of opentimelineio in one
# process do not share their type information, which is the thing shared
# libraries were turned on to avoid.
set_tests_properties(tl-test PROPERTIES ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:${CMAKE_INSTALL_PREFIX}/python/opentimelineio")

# The player's audio, checked sample by sample, on its own: SDL's disk audio
# driver writes what the device is given to a file rather than to hardware,
# so this runs without a sound card, and the driver is chosen for the whole
# process when SDL starts. Run with the rest of the suite it says it is
# skipped.
add_test(
    NAME tl-test-audio
    COMMAND tl-test -log -noGL timeline_tests::PlayerAudioTest)
set_tests_properties(tl-test-audio PROPERTIES
    ENVIRONMENT "SDL_AUDIO_DRIVER=disk;SDL_AUDIO_DISK_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/tl-test-audio.raw"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_INSTALL_PREFIX}/python/opentimelineio")
