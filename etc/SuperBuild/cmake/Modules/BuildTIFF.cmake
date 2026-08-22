include(ExternalProject)

# The release tarball rather than a clone of the repository. gitlab.com
# answers git over HTTPS with 403 often enough to fail a build -- three
# attempts in a row, on a runner that had done nothing else -- and this is
# the project's own download host, which does not.
set(TIFF_VERSION "4.7.2")
set(TIFF_URL "https://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz")
set(TIFF_HASH "SHA256=672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a")

set(TIFF_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -Dtiff-tools=OFF
    -Dtiff-tests=OFF
    -Dtiff-contrib=OFF
    -Dtiff-docs=OFF
    -Dzstd=OFF
    -Dlibdeflate=OFF
    -Djbig=OFF
    -Djpeg=OFF
    -Dold-jpeg=OFF
    -Djpeg12=OFF
    -Dlerc=OFF
    -Dlzma=OFF
    -Dwebp=OFF)

ExternalProject_Add(
    TIFF
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/TIFF
    URL ${TIFF_URL}
    URL_HASH ${TIFF_HASH}
    LIST_SEPARATOR |
    CMAKE_ARGS ${TIFF_ARGS})
