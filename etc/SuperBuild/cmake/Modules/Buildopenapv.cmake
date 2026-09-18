include(ExternalProject)

# The reference implementation of APV, and the only encoder FFmpeg has for
# it: the "apv" codec FFmpeg carries itself decodes only. FFmpeg 9 calls
# oapvm_create() with the arguments it took before 1.0, which is what
# FFmpeg-patch/liboapv.patch is for.
set(openapv_VERSION "1.1.1.0")
set(openapv_URL "https://github.com/AcademySoftwareFoundation/openapv/archive/refs/tags/v${openapv_VERSION}.tar.gz")
set(openapv_HASH "SHA256=956e6e2cc822c63af4c323bf86464f1186171314e67e9c5153f58bd875538470")

# Static, like the other codecs here, and position independent because
# FFmpeg links it into shared libraries. The command line applications are
# the project's own front end; FFmpeg is ours.
#
# The MSVC runtime redistributables are not this dependency's to install:
# its CPack setup pulls in InstallRequiredSystemLibraries, which would
# otherwise write them into the shared prefix.
set(openapv_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOAPV_BUILD_APPS=OFF
    -DOAPV_BUILD_SHARED_LIB=OFF
    -DOAPV_BUILD_STATIC_LIB=ON
    -DENABLE_TESTS=OFF
    -DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

# To send upstream; drop it once a release carries the fix.
#
# An MSVC build compiles and runs, but installs no pkg-config file, which
# is how FFmpeg finds the library, and gives the AVX sources no /arch:AVX2
# (openapv-patch/msvc.patch).
find_package(Git REQUIRED)

ExternalProject_Add(
    openapv
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openapv
    URL ${openapv_URL}
    URL_HASH ${openapv_HASH}
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DPATCH_SOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}/openapv/src/openapv
        -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/openapv-patch/msvc.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake
    LIST_SEPARATOR |
    CMAKE_ARGS ${openapv_ARGS})
