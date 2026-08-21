include(ExternalProject)

# The tag's tarball rather than a clone, for the same reason as TIFF: this
# project also lives on gitlab.com, whose git over HTTPS returns 403 often
# enough to fail a build. The GitHub repository is the project's own mirror,
# and NASM is already fetched from a GitHub tag archive here.
set(svt-av1_VERSION "4.0.1")
set(svt-av1_URL "https://github.com/AOMediaCodec/SVT-AV1/archive/refs/tags/v${svt-av1_VERSION}.tar.gz")
set(svt-av1_HASH "SHA256=fb3e983b82955575e01bce260d2bee6551f329db8154e4a0f69947093354d3dc")

set(svt-av1_DEPS)
if(TLRENDER_NASM)
    list(APPEND svt-av1_DEPS NASM)
endif()

set(svt-av1_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DBUILD_APPS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON)
if(TLRENDER_NASM)
    list(APPEND svt-av1_ARGS -DCMAKE_ASM_NASM_COMPILER=${CMAKE_INSTALL_PREFIX}/bin/nasm)
endif()

ExternalProject_Add(
    svt-av1
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/svt-av1
    DEPENDS ${svt-av1_DEPS}
    URL ${svt-av1_URL}
    URL_HASH ${svt-av1_HASH}
    CMAKE_ARGS ${svt-av1_ARGS})
