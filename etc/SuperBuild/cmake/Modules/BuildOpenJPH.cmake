include(ExternalProject)

# The HTJ2K codec behind OpenEXR's HTJ2K compression. OpenEXR carries a
# vendored copy and falls back to it silently when it cannot find an
# installed one, so this is built into the prefix ahead of OpenEXR and
# found there -- one copy, at a version written down here.
set(OpenJPH_VERSION "0.31.0")
set(OpenJPH_URL "https://github.com/aous72/OpenJPH/archive/refs/tags/${OpenJPH_VERSION}.tar.gz")
set(OpenJPH_HASH "SHA256=fe169dbbaae71a169a0a6a68dccb346616193252c1ca044217afa0d5d1dc436f")

set(OpenJPH_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOJPH_BUILD_EXECUTABLES=OFF
    -DOJPH_BUILD_TESTS=OFF
    # The codec does not read or write TIFF; only the command line tools
    # above do, and they are not built.
    -DOJPH_ENABLE_TIFF_SUPPORT=OFF)

ExternalProject_Add(
    OpenJPH
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/OpenJPH
    URL ${OpenJPH_URL}
    URL_HASH ${OpenJPH_HASH}
    LIST_SEPARATOR |
    CMAKE_ARGS ${OpenJPH_ARGS})
