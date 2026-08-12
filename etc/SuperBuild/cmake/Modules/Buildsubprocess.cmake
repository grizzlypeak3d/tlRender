include(ExternalProject)

set(subprocess_GIT_REPOSITORY "https://github.com/sheredom/subprocess.h.git")
set(subprocess_GIT_TAG "9ce0d701b6fb10f8f8c4445edd31e7c60a1237e3")

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
