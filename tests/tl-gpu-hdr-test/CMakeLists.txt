add_executable(tl-gpu-hdr-test tl-gpu-hdr-test.cpp)
target_link_libraries(tl-gpu-hdr-test tlGPU)
set_target_properties(tl-gpu-hdr-test PROPERTIES FOLDER tests)

# Skipped, exit code 77, where there is no GPU device to draw with.
add_test(NAME tl-gpu-hdr-test COMMAND tl-gpu-hdr-test)
set_tests_properties(tl-gpu-hdr-test PROPERTIES SKIP_RETURN_CODE 77)
