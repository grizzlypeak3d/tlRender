include(ExternalProject)

set(libjpeg-turbo_GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo.git")
set(libjpeg-turbo_GIT_TAG "3.1.1")

set(libjpeg-turbo_DEPS)
if(TLRENDER_NASM)
    set(libjpeg-turbo_DEPS ${libjpeg-turbo_DEPS} NASM)
endif()

set(libjpeg-turbo_ENABLE_SHARED ON)
set(libjpeg-turbo_ENABLE_STATIC OFF)
if(NOT BUILD_SHARED_LIBS)
    set(libjpeg-turbo_ENABLE_SHARED OFF)
    set(libjpeg-turbo_ENABLE_STATIC ON)
endif()

set(libjpeg-turbo_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DENABLE_SHARED=${libjpeg-turbo_ENABLE_SHARED}
    -DENABLE_STATIC=${libjpeg-turbo_ENABLE_STATIC}
    -DWITH_TESTS=OFF)
if(TLRENDER_NASM)
    list(APPEND libjpeg-turbo_ARGS -DCMAKE_ASM_NASM_COMPILER=${CMAKE_INSTALL_PREFIX}/bin/nasm)
endif()

ExternalProject_Add(
    libjpeg-turbo
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo
    DEPENDS ${libjpeg-turbo_DEPS}
    GIT_REPOSITORY ${libjpeg-turbo_GIT_REPOSITORY}
    GIT_TAG ${libjpeg-turbo_GIT_TAG}
    LIST_SEPARATOR |
    CMAKE_ARGS ${libjpeg-turbo_ARGS})

# The command line tools go with the library and there is no option to
# leave them out, so they are removed after the install. Only the library
# is wanted; nothing here runs cjpeg.
set(libjpeg-turbo_TOOLS)
foreach(tool cjpeg djpeg jpegtran rdjpgcom wrjpgcom tjbench)
    list(APPEND libjpeg-turbo_TOOLS
        ${CMAKE_INSTALL_PREFIX}/bin/${tool}${CMAKE_EXECUTABLE_SUFFIX})
endforeach()
ExternalProject_Add_Step(
    libjpeg-turbo libjpeg-turbo-tools
    COMMAND ${CMAKE_COMMAND} -E rm -f ${libjpeg-turbo_TOOLS}
    DEPENDEES install
    ALWAYS OFF)
