include(ExternalProject)

set(USD_DEPS)

set(USD_GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git)
set(USD_GIT_TAG v26.05)

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LC)
set(USD_ARGS -v --build-variant ${CMAKE_BUILD_TYPE_LC})
if(CMAKE_OSX_ARCHITECTURES)
    list(APPEND USD_ARGS --build-target ${CMAKE_OSX_ARCHITECTURES})
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND USD_ARGS --build-args)
    list(APPEND USD_ARGS USD,-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    list(APPEND USD_ARGS OpenSubdiv,-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    list(APPEND USD_ARGS MaterialX,-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    list(APPEND USD_ARGS
        oneTBB,-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        oneTBB,-DCMAKE_CXX_OSX_DEPLOYMENT_TARGET_FLAG:STRING="-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        oneTBB,-DCMAKE_C_OSX_DEPLOYMENT_TARGET_FLAG:STRING="-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
list(APPEND USD_ARGS --no-python --no-examples --no-tutorials --no-tools)
list(APPEND USD_ARGS --onetbb)
list(APPEND USD_ARGS --verbose)

set(USD_INSTALL_COMMAND)
if(WIN32)
    # \todo On Windows the USD cmake build system installs the "*.dll" files
    # and "usd" directory into "lib", however it seems like they need to be
    # in "bin" instead.
    cmake_path(CONVERT ${CMAKE_INSTALL_PREFIX} TO_NATIVE_PATH_LIST CMAKE_INSTALL_PREFIX_NATIVE)
    set(USD_INSTALL_COMMAND
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_INSTALL_PREFIX}/lib/usd  ${CMAKE_INSTALL_PREFIX}/bin/usd
        COMMAND copy /Y "${CMAKE_INSTALL_PREFIX_NATIVE}\\lib\\*.dll" "${CMAKE_INSTALL_PREFIX_NATIVE}\\bin")
endif()

ExternalProject_Add(
    USD
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/USD
    DEPENDS ${USD_DEPS}
    GIT_REPOSITORY ${USD_GIT_REPOSITORY}
    GIT_TAG ${USD_GIT_TAG}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${TLRENDER_USD_PYTHON} build_scripts/build_usd.py ${USD_ARGS} ${CMAKE_INSTALL_PREFIX}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND "${USD_INSTALL_COMMAND}")
