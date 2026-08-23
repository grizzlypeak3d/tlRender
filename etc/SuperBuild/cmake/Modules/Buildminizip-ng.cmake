include(ExternalProject)

set(minizip-ng_GIT_REPOSITORY "https://github.com/zlib-ng/minizip-ng.git")
set(minizip-ng_GIT_TAG "4.0.10")

# A shared build on Windows needs something to export. This library carries no
# __declspec(dllexport) of its own, so it builds a DLL that exports nothing and
# the linker writes no import library beside it; the first thing to ask for one
# stops:
#
#     LINK : fatal error LNK1104: cannot open file 'minizip-ng.lib'
#
# CMake writes a module definition file listing what the objects hold instead.
# Set here rather than for every dependency: the ones that carry export macros
# already do not need it, and turning it on for them breaks the ones with
# assembly, where the generator cannot read the objects:
#
#     unrecognized file format in '.../intrapred_asm_sse2.obj'
#
# It covers functions and not data, so if a global ever has to be exported this
# has to become a static build instead. Ignored everywhere but Windows.
set(minizip-ng_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=${BUILD_SHARED_LIBS}
    -DMZ_COMPAT=OFF
    -DMZ_BZIP2=OFF
    -DMZ_LZMA=OFF
    -DMZ_ZSTD=OFF
    -DMZ_LIBCOMP=OFF
    -DMZ_FETCH_LIBS=OFF
    -DMZ_PKCRYPT=OFF
    -DMZ_WZAES=OFF
    -DMZ_OPENSSL=OFF
    -DMZ_BCRYPT=OFF
    -DMZ_LIBBSD=OFF
    -DMZ_ICONV=OFF)

ExternalProject_Add(
    minizip-ng
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/minizip-ng
    GIT_REPOSITORY ${minizip-ng_GIT_REPOSITORY}
    GIT_TAG ${minizip-ng_GIT_TAG}
    LIST_SEPARATOR |
    CMAKE_ARGS ${minizip-ng_ARGS})
