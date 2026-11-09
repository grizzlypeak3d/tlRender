add_executable(tl-gpu-ocio-test tl-gpu-ocio-test.cpp)
# SDL by way of the library that found it.
target_link_libraries(tl-gpu-ocio-test OpenColorIO::OpenColorIO ftk::ftkGPU)
set_target_properties(tl-gpu-ocio-test PROPERTIES FOLDER tests)

# Skipped, exit code 77, where there is no GPU device to draw with.
add_test(NAME tl-gpu-ocio-test COMMAND tl-gpu-ocio-test)
set_tests_properties(tl-gpu-ocio-test PROPERTIES SKIP_RETURN_CODE 77)
