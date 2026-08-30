add_executable(player player.cpp)
target_link_libraries(player tlUI)
set_target_properties(player PROPERTIES FOLDER examples)

if(EMSCRIPTEN)
    target_link_options(player PRIVATE
        # The workers are preallocated lazily: a thread started from
        # the main thread cannot wait for one to spawn without
        # deadlocking the event loop, and instantiating every worker
        # before main() leaves the page black for as long as that
        # takes.
        "-sPTHREAD_POOL_SIZE=32"
        "-sPTHREAD_POOL_DELAY_LOAD=1"
        "-sALLOW_MEMORY_GROWTH=1"
        # The default 2GB maximum stays: growing past it turned into
        # out-of-bounds crashes in Firefox once the heap crossed 2GB --
        # a pointer-sign bug somewhere in the stack, unfound.
        "-sMAXIMUM_MEMORY=2GB"
        "-sFULL_ES3=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "--profiling-funcs")
    # The page and the decode worker; mp4box.js is the demuxer the
    # worker loads, and the coi service worker supplies the isolation
    # headers on hosts that cannot send them.
    add_custom_command(TARGET player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/lib/tlRender/IO/WebCodecsWorker.js
            ${PROJECT_SOURCE_DIR}/etc/Web/mp4box.all.min.js
            ${PROJECT_SOURCE_DIR}/etc/Web/coi-serviceworker.js
            $<TARGET_FILE_DIR:player>)
endif()
