add_executable(tl-gpu-hdr-test tl-gpu-hdr-test.cpp)
target_link_libraries(tl-gpu-hdr-test tlGPU)
set_target_properties(tl-gpu-hdr-test PROPERTIES FOLDER tests)
