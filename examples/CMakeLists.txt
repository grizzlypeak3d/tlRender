add_subdirectory(player)
if(NOT EMSCRIPTEN)
    add_subdirectory(test-patterns)
endif()

if(EMSCRIPTEN)
    # The page asks for its .js and .wasm with the build id, so a cached
    # page always gets the pair it was built with -- a page and a binary
    # from different deployments abort on load.
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE tl_WEB_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT tl_WEB_ID)
        set(tl_WEB_ID ${CMAKE_PROJECT_VERSION})
    endif()
    # Each web example gets a page to run in; the server has to send
    # the COOP/COEP headers, since wasm threads need cross-origin
    # isolation.
    foreach(tl_WEB_APP player)
        configure_file(
            ${PROJECT_SOURCE_DIR}/etc/Web/index.html.in
            ${CMAKE_CURRENT_BINARY_DIR}/${tl_WEB_APP}/index.html
            @ONLY)
    endforeach()
endif()
