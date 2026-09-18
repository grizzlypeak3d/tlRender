include(ExternalProject)

# The reference implementation of APV, and the only encoder FFmpeg has for
# it: the "apv" codec FFmpeg carries itself decodes only.
#
# 0.3.0.0 rather than the current 1.1.1.0, which FFmpeg 9 cannot build
# against: oapvm_create() took a description argument in 1.0, and FFmpeg's
# wrapper still calls the older form. Worth moving on when FFmpeg does.
set(openapv_VERSION "0.3.0.0")
set(openapv_URL "https://github.com/AcademySoftwareFoundation/openapv/archive/refs/tags/v${openapv_VERSION}.tar.gz")
set(openapv_HASH "SHA256=dc5cd1618a07e8b340e12562cae37d612b3a1467ee80d986c477165ae602a37e")

# Static, like the other codecs here, and position independent because
# FFmpeg links it into shared libraries. The command line applications are
# the project's own front end; FFmpeg is ours.
set(openapv_ARGS
    ${TLRENDER_EXTERNAL_ARGS}
    -DOAPV_BUILD_APPS=OFF
    -DOAPV_BUILD_SHARED_LIB=OFF
    -DOAPV_BUILD_STATIC_LIB=ON
    -DENABLE_TESTS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

ExternalProject_Add(
    openapv
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openapv
    URL ${openapv_URL}
    URL_HASH ${openapv_HASH}
    LIST_SEPARATOR |
    CMAKE_ARGS ${openapv_ARGS})

# The static library installs into a directory of its own, and the
# pkg-config file that comes with it says the library directory above --
# so "pkg-config --libs oapv" names a path the library is not on, and
# FFmpeg's configure check for it fails. Put a copy where the file says
# it is rather than editing the file, which would have to be rewritten
# for every layout it is asked about.
ExternalProject_Add_Step(
    openapv
    lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_INSTALL_PREFIX}/lib/oapv/liboapv.a
        ${CMAKE_INSTALL_PREFIX}/lib/liboapv.a
    DEPENDEES install
    ALWAYS OFF
    COMMENT "Copying liboapv where its pkg-config file says it is")
