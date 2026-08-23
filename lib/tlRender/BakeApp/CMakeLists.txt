set(HEADERS
    App.h)

set(SOURCE
    App.cpp)

# Static. This is one program's code kept in a library so the tests can reach
# it, linked by tlbake and nothing else, and it carries no export macros. A
# shared build on Windows would give it a library that exports nothing and no
# import library to link against:
#
#     LINK : fatal error LNK1104: cannot open file 'tlBakeApp.lib'
#
# There is no third party to import this, so there is nothing to mark.
add_library(tlBakeApp STATIC ${HEADERS} ${SOURCE})
set_target_properties(tlBakeApp PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tlBakeApp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(tlBakeApp tlGL)

set_target_properties(tlBakeApp PROPERTIES FOLDER lib)
set_target_properties(tlBakeApp PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS tlBakeApp
    ARCHIVE DESTINATION lib COMPONENT dev
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/tlRender/BakeApp COMPONENT dev)
