set(HEADERS
    Export.h
    Render.h)
set(PRIVATE_HEADERS
    RenderPrivate.h)

set(SOURCE
    Render.cpp
    RenderShaders.cpp
    RenderVideo.cpp)

add_library(tlGPU ${HEADERS} ${PRIVATE_HEADERS} ${SOURCE})
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlGPU PRIVATE TL_GPU_EXPORTS)
else()
    target_compile_definitions(tlGPU PUBLIC TL_GPU_STATIC)
endif()
target_compile_definitions(tlGPU PUBLIC TLRENDER_GPU)

target_include_directories(tlGPU
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlGPU PUBLIC tlTimeline ftk::ftkGPU)

set_target_properties(tlGPU PROPERTIES FOLDER lib)
set_target_properties(tlGPU PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlGPU
    EXPORT tlGPUTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/GPU COMPONENT dev)
install(
    EXPORT tlGPUTargets
    FILE tlGPUTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
