add_executable(tl-gpu-ocio-test tl-gpu-ocio-test.cpp)
target_link_libraries(tl-gpu-ocio-test OpenColorIO::OpenColorIO SDL3::SDL3)
set_target_properties(tl-gpu-ocio-test PROPERTIES FOLDER tests)
