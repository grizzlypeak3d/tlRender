# Apply the OpenTimelineIO patch, and do nothing if it is already applied.
#
# The patch step runs again whenever its stamp is cleared, which is not always
# accompanied by a fresh clone, and "git apply" fails on a tree it has already
# been applied to. Asking whether it reverses tells the two apart: a tree that
# the patch can be taken back out of is a tree it is already in.
#
# Run with -DOTIO_SOURCE_DIR=... -DOTIO_PATCH=... -DGIT_EXECUTABLE=...

execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${OTIO_PATCH}
    WORKING_DIRECTORY ${OTIO_SOURCE_DIR}
    RESULT_VARIABLE alreadyApplied
    OUTPUT_QUIET ERROR_QUIET)
if(alreadyApplied EQUAL 0)
    message(STATUS "OTIO patch is already applied")
    return()
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} apply ${OTIO_PATCH}
    WORKING_DIRECTORY ${OTIO_SOURCE_DIR}
    RESULT_VARIABLE result
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    # Most likely the change has landed upstream, or OTIO_GIT_TAG moved and the
    # file the patch expects is no longer what is there. Either way the patch
    # has to be looked at rather than worked around: see the notes in
    # BuildOTIO.cmake.
    message(FATAL_ERROR
        "Could not apply ${OTIO_PATCH}:\n${error}\n"
        "If OTIO_GIT_TAG moved, re-make the patch against the new source. If "
        "the change is upstream now, drop the patch and this step with it.")
endif()
message(STATUS "OTIO patch applied")
