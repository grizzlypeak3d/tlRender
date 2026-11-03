set(HEADERS
    Engine.h
    EngineCpp.h
    Export.h)
set(SOURCE
    Bundle.cpp
    Engine.cpp)

# The C interface to the engine, for Swift and any other language that
# speaks C. It draws with the GPU renderer only, and hosts the timeline
# widget.
add_library(tlC ${HEADERS} ${SOURCE})
target_link_libraries(tlC PUBLIC tlGPU tlUI)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(tlC PRIVATE TL_C_EXPORTS)
else()
    target_compile_definitions(tlC PUBLIC TL_C_STATIC)
endif()
set_target_properties(tlC PROPERTIES FOLDER lib)
set_target_properties(tlC PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(
    TARGETS tlC
    EXPORT tlCTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/C)
install(
    FILES module.modulemap
    DESTINATION include/tlRender/C)
install(
    EXPORT tlCTargets
    FILE tlCTargets.cmake
    DESTINATION "lib/cmake/tlRender"
    NAMESPACE tlRender::)
