# Run with "cmake -P" after OpenTimelineIO installs. See BuildOTIO.cmake for
# why this is here.
#
# Expects OTIO_DYLIB_DIR, where OTIO put its libraries, OTIO_LIB_DIR, the
# prefix's lib, and OTIO_APPLE for whether the install names need rewriting.

# The two names do not overlap: "libopentime." is not a prefix of
# "libopentimelineio". Everything after it is a version, an extension or
# both, and the extension is checked below.
file(GLOB otioGlob
    "${OTIO_DYLIB_DIR}/libopentime.*"
    "${OTIO_DYLIB_DIR}/libopentimelineio.*")
set(otioLibs)
foreach(otioFile ${otioGlob})
    if(otioFile MATCHES "\\.(dylib|so)(\\.[0-9.]+)?$")
        list(APPEND otioLibs "${otioFile}")
    endif()
endforeach()

# Only macOS carries a path inside the library. On other platforms the name
# recorded is the bare one and the rpath finds it, so the links below are the
# whole of it.
if(OTIO_APPLE)
    foreach(otioLib ${otioLibs})
        # The links beside it share its install name; only the file itself
        # carries one to change.
        if(IS_SYMLINK "${otioLib}")
            continue()
        endif()

        # Read and rewrite rather than spell the name out: the version is part
        # of it, and one written down here would go stale without saying so.
        execute_process(
            COMMAND otool -D "${otioLib}"
            OUTPUT_VARIABLE otioIdOutput
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE ".*\n" "" otioId "${otioIdOutput}")
        if(NOT otioId MATCHES "^@loader_path/")
            continue()
        endif()
        string(REPLACE "@loader_path/" "@rpath/" otioNewId "${otioId}")

        message(STATUS "OTIO install name: ${otioId} -> ${otioNewId}")
        execute_process(
            COMMAND install_name_tool -id "${otioNewId}" "${otioLib}"
            RESULT_VARIABLE otioResult)
        if(NOT otioResult EQUAL 0)
            message(FATAL_ERROR "Could not set the install name of ${otioLib}")
        endif()
    endforeach()
endif()

# The Python modules name Imath by rpath but carry no rpath to resolve it
# with, so importing opentimelineio on its own cannot load; importing after
# something that already pulled Imath in masks it. The rpath added is
# relative to the prefix's lib, keeping the install relocatable. The
# libraries need none of their own: dyld resolves their references with the
# rpaths of the module whose import loaded them.
if(OTIO_APPLE)
    file(GLOB otioModules
        "${OTIO_DYLIB_DIR}/_otio.*.so"
        "${OTIO_DYLIB_DIR}/_opentime.*.so")
    file(RELATIVE_PATH otioLibRel "${OTIO_DYLIB_DIR}" "${OTIO_LIB_DIR}")
    foreach(otioModule ${otioModules})
        set(otioRPath "@loader_path/${otioLibRel}")
        execute_process(
            COMMAND otool -l "${otioModule}"
            OUTPUT_VARIABLE otioLoadOutput)
        if(otioLoadOutput MATCHES "path ${otioRPath} ")
            continue()
        endif()
        message(STATUS "OTIO rpath: ${otioModule} += ${otioRPath}")
        execute_process(
            COMMAND install_name_tool -add_rpath "${otioRPath}" "${otioModule}"
            RESULT_VARIABLE otioResult)
        if(NOT otioResult EQUAL 0)
            message(FATAL_ERROR "Could not add the rpath of ${otioModule}")
        endif()
    endforeach()
endif()

# Linked into the prefix's lib rather than copied there. Two images of
# libopentimelineio in one process do not share their type information, which
# is the thing shared libraries were turned on to avoid.
if(NOT OTIO_DYLIB_DIR STREQUAL OTIO_LIB_DIR)
    foreach(otioLib ${otioLibs})
        get_filename_component(otioName "${otioLib}" NAME)
        file(CREATE_LINK "${otioLib}" "${OTIO_LIB_DIR}/${otioName}" SYMBOLIC)
        message(STATUS "OTIO link: lib/${otioName}")
    endforeach()
endif()
