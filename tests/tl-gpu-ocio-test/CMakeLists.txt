add_executable(tl-gpu-ocio-test tl-gpu-ocio-test.cpp)
# SDL by way of the library that found it.
target_link_libraries(tl-gpu-ocio-test OpenColorIO::OpenColorIO ftk::ftkGPU)
set_target_properties(tl-gpu-ocio-test PROPERTIES FOLDER tests)
