set(HEADERS
    Render.h)
set(PRIVATE_HEADERS
    RenderPrivate.h)

set(SOURCE
    Render.cpp
    RenderPrims.cpp
    RenderVideo.cpp)
if("${ftk_API}" STREQUAL "GL_4_1" OR "${ftk_API}" STREQUAL "GL_4_1_Debug")
    list(APPEND SOURCE RenderShaders_GL_4_1.cpp)
elseif("${ftk_API}" STREQUAL "GLES_2")
    list(APPEND SOURCE RenderShaders_GLES_2.cpp)
endif()

add_library(tlGL ${HEADERS} ${PRIVATE_HEADERS} ${SOURCE})

target_include_directories(tlGL
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlGL tlTimeline)

set_target_properties(tlGL PROPERTIES FOLDER lib)
set_target_properties(tlGL PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlGL
    EXPORT tlGLTargets
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/GL COMPONENT dev)
install(
    EXPORT tlGLTargets
    FILE tlGLTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::
    COMPONENT dev)
