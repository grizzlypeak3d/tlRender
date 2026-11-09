add_executable(tl-gpu-color-test tl-gpu-color-test.cpp)
target_link_libraries(tl-gpu-color-test tlGPU)
target_compile_definitions(tl-gpu-color-test PRIVATE TLRENDER_SAMPLE_DATA="${PROJECT_SOURCE_DIR}/etc/SampleData")
set_target_properties(tl-gpu-color-test PROPERTIES FOLDER tests)

# Skipped, exit code 77, where there is no GPU device to draw with.
add_test(NAME tl-gpu-color-test COMMAND tl-gpu-color-test)
set_tests_properties(tl-gpu-color-test PROPERTIES SKIP_RETURN_CODE 77)
