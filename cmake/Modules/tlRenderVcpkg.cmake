# Locate the vcpkg checkout. Prefer the in-tree submodule, fall back to
# VCPKG_ROOT env var if a developer prefers a shared install.
set(_tlr_vcpkg_root "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_tlr_vcpkg_root "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
elseif(DEFINED ENV{VCPKG_ROOT}
       AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_tlr_vcpkg_root "$ENV{VCPKG_ROOT}")
else()
    message(FATAL_ERROR
        "No vcpkg checkout was found. Either run "
        "'git submodule update --init vcpkg' in the source tree, "
        "or set the VCPKG_ROOT environment variable to a vcpkg clone.")
endif()

# Bootstrap vcpkg if its binary isn't built yet. Cheap no-op when already built.
if(WIN32)
    set(_tlr_vcpkg_exe "${_tlr_vcpkg_root}/vcpkg.exe")
    set(_tlr_vcpkg_bootstrap "${_tlr_vcpkg_root}/bootstrap-vcpkg.bat")
else()
    set(_tlr_vcpkg_exe "${_tlr_vcpkg_root}/vcpkg")
    set(_tlr_vcpkg_bootstrap "${_tlr_vcpkg_root}/bootstrap-vcpkg.sh")
endif()
if(NOT EXISTS "${_tlr_vcpkg_exe}" AND EXISTS "${_tlr_vcpkg_bootstrap}")
    message(STATUS "Bootstrapping vcpkg at ${_tlr_vcpkg_root}")
    execute_process(
        COMMAND "${_tlr_vcpkg_bootstrap}" -disableMetrics
        WORKING_DIRECTORY "${_tlr_vcpkg_root}"
        RESULT_VARIABLE _tlr_bootstrap_result)
    if(NOT _tlr_bootstrap_result EQUAL 0)
        message(FATAL_ERROR "vcpkg bootstrap failed (exit ${_tlr_bootstrap_result})")
    endif()
endif()

# Pick the dynamic triplet for the host platform if one wasn't specified.
# We default to *-dynamic everywhere because FFmpeg must ship as shared
# libraries to satisfy the LGPL relinking requirement.
#
# This runs BEFORE project(), so CMake's normal architecture detection
# variables (CMAKE_HOST_SYSTEM_PROCESSOR, CMAKE_SYSTEM_PROCESSOR) aren't
# populated yet. We use uname directly and respect CMAKE_OSX_ARCHITECTURES
# when the build script passes it explicitly.
if(NOT DEFINED VCPKG_TARGET_TRIPLET AND NOT DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows-dynamic" CACHE STRING "")
    elseif(APPLE)
        # Honor CMAKE_OSX_ARCHITECTURES if the user/build script set it.
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx-dynamic" CACHE STRING "")
        elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
            set(VCPKG_TARGET_TRIPLET "x64-osx-dynamic" CACHE STRING "")
        else()
            # Detect the host arch via uname (reliable before project()).
            execute_process(COMMAND uname -m
                OUTPUT_VARIABLE _tlr_host_arch
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(_tlr_host_arch STREQUAL "arm64")
                set(VCPKG_TARGET_TRIPLET "arm64-osx-dynamic" CACHE STRING "")
            else()
                set(VCPKG_TARGET_TRIPLET "x64-osx-dynamic" CACHE STRING "")
            endif()
        endif()
    elseif(UNIX)
        set(VCPKG_TARGET_TRIPLET "x64-linux-dynamic" CACHE STRING "")
    endif()
endif()

# Point at our overlay triplets dir so the *-dynamic triplets resolve.
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets"
    CACHE STRING "" FORCE)

# Finally, set the toolchain file. Must happen before project().
set(CMAKE_TOOLCHAIN_FILE
    "${_tlr_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

message(STATUS "tlRender: using vcpkg at ${_tlr_vcpkg_root}")
message(STATUS "tlRender: VCPKG_TARGET_TRIPLET = ${VCPKG_TARGET_TRIPLET}")
