set(HEADERS
    Export.h
    Render.h)
set(PRIVATE_HEADERS
    RenderPrivate.h)

set(SOURCE
    Render.cpp
    RenderPrims.cpp
    RenderVideo.cpp)
if("${ftk_API}" STREQUAL "GL_4_1" OR "${ftk_API}" STREQUAL "GL_4_1_Debug")
    list(APPEND SOURCE RenderShaders_GL_4_1.cpp)
elseif("${ftk_API}" STREQUAL "GLES_3")
    list(APPEND SOURCE RenderShaders_GLES_3.cpp)
endif()

add_library(tlGL ${HEADERS} ${PRIVATE_HEADERS} ${SOURCE})
# Private when building, so this library's own API is dllexport here and
# dllimport to everything else. Public when static, which a consumer does
# have to know: the API then carries no declspec at all.
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlGL PRIVATE TL_GL_EXPORTS)
else()
    target_compile_definitions(tlGL PUBLIC TL_GL_STATIC)
endif()

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
