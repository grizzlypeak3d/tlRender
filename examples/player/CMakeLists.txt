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
        # The page sets the pool size (its "pool" query; 32 otherwise)
        # and starts main() once the pool has loaded; see
        # etc/Web/index.html.in. No "||" fallback here: the link
        # command may go through a shell.
        "-sPTHREAD_POOL_SIZE=globalThis.__tlPoolSize"
        "-sPTHREAD_POOL_DELAY_LOAD=1"
        "-sEXPORTED_RUNTIME_METHODS=callMain"
        "-sALLOW_MEMORY_GROWTH=1"
        # The default 2GB maximum stays: growing past it turned into
        # out-of-bounds crashes in Firefox once the heap crossed 2GB --
        # a pointer-sign bug somewhere in the stack, unfound.
        "-sMAXIMUM_MEMORY=2GB"
        "-sFULL_ES3=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        # The timeline reader fetches .otio files, since there is no
        # file system on the web.
        "-sFETCH"
        # The page makes the wasm memory itself, so the maximum it may
        # grow to can follow the device (see etc/Web/index.html.in); the
        # runtime only takes one from the page when it is on this list,
        # which is otherwise the default.
        "-sINCOMING_MODULE_JS_API=ENVIRONMENT,arguments,canvas,dynamicLibraries,elementPointerLock,instantiateWasm,locateFile,monitorRunDependencies,noExitRuntime,noInitialRun,onAbort,onExit,onRuntimeInitialized,postRun,preInit,preRun,print,printErr,setStatus,statusMessage,stderr,stdin,stdout,thisProgram,wasm,websocket,wasmMemory"
        "--profiling-funcs")
    # The page and the decode worker; mp4box.js is the demuxer the
    # worker loads, and the coi service worker supplies the isolation
    # headers on hosts that cannot send them.
    add_custom_command(TARGET player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/lib/tlRender/IO/WebCodecsWorker.js
            ${PROJECT_SOURCE_DIR}/etc/Web/mp4box.all.min.js
            ${PROJECT_SOURCE_DIR}/etc/Web/coi-serviceworker.js
            ${PROJECT_SOURCE_DIR}/etc/Web/test.mp4
            ${PROJECT_SOURCE_DIR}/etc/Web/test.otio
            ${PROJECT_SOURCE_DIR}/etc/Web/StEM2_Web.otio
            $<TARGET_FILE_DIR:player>)
endif()
