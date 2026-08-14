include(ExternalProject)

set(subprocess_GIT_REPOSITORY "https://github.com/sheredom/subprocess.h.git")
# Includes the fix for close-on-exec pipes; see the note in FFmpegCmd.cpp.
set(subprocess_GIT_TAG "0d76f78ff8b56d1240ffe6571d689c5e26299527")

ExternalProject_Add(
    subprocess
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/subprocess
    GIT_REPOSITORY ${subprocess_GIT_REPOSITORY}
    GIT_TAG ${subprocess_GIT_TAG}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/include
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/subprocess/src/subprocess/subprocess.h
            ${CMAKE_INSTALL_PREFIX}/include/subprocess.h)
