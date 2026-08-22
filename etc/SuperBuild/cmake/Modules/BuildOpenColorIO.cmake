include(ExternalProject)

set(OpenColorIO_GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/OpenColorIO.git")
set(OpenColorIO_GIT_TAG "v2.5.2")

set(OpenColorIO_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOCIO_BUILD_APPS=OFF
    -DOCIO_BUILD_TESTS=OFF
    -DOCIO_BUILD_GPU_TESTS=OFF
    -DOCIO_BUILD_PYTHON=OFF
    -DOCIO_INSTALL_EXT_PACKAGES=NONE)
cmake_host_system_information(RESULT HAS_SSE2 QUERY HAS_SSE2)
list(APPEND OpenColorIO_ARGS -DOCIO_USE_SSE2=${HAS_SSE2})
#if(APPLE)
#    execute_process(
#        COMMAND uname -m
#        RESULT_VARIABLE result
#        OUTPUT_VARIABLE OpenColorIO_OSX_NATIVE_ARCH
#        OUTPUT_STRIP_TRAILING_WHITESPACE)
#    list(APPEND OpenColorIO_ARGS -DCMAKE_OSX_ARCHITECTURES=${OpenColorIO_OSX_NATIVE_ARCH})
#endif()

ExternalProject_Add(
    OpenColorIO
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OpenColorIO
    DEPENDS Imath yaml-cpp expat pystring minizip-ng
    GIT_REPOSITORY ${OpenColorIO_GIT_REPOSITORY}
    GIT_TAG ${OpenColorIO_GIT_TAG}
    LIST_SEPARATOR |
    CMAKE_ARGS ${OpenColorIO_ARGS})
