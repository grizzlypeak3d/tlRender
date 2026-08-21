# Run with "cmake -P" after OpenTimelineIO installs, on macOS only. See
# BuildOTIO.cmake for why this is here.
#
# Expects OTIO_DYLIB_DIR, where OTIO put its libraries, and OTIO_LIB_DIR, the
# prefix's lib.
#
# The install name is rewritten by reading what is there and swapping
# "@loader_path" for "@rpath", rather than by naming the files: the version is
# part of the name, and a version that is written down here goes stale without
# saying so.

file(GLOB otioLibs
    "${OTIO_DYLIB_DIR}/libopentime.*.dylib"
    "${OTIO_DYLIB_DIR}/libopentimelineio.*.dylib")

foreach(otioLib ${otioLibs})
    # The symlinks beside it share its install name; only the file itself
    # carries one to change.
    if(IS_SYMLINK "${otioLib}")
        continue()
    endif()

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

# Linked into the prefix's lib rather than copied there. Two images of
# libopentimelineio in one process do not share their type information, which
# is the thing shared libraries were turned on to avoid.
if(NOT OTIO_DYLIB_DIR STREQUAL OTIO_LIB_DIR)
    file(GLOB otioAll
        "${OTIO_DYLIB_DIR}/libopentime.*dylib"
        "${OTIO_DYLIB_DIR}/libopentimelineio.*dylib")
    foreach(otioLib ${otioAll})
        get_filename_component(otioName "${otioLib}" NAME)
        file(CREATE_LINK "${otioLib}" "${OTIO_LIB_DIR}/${otioName}" SYMBOLIC)
    endforeach()
endif()
