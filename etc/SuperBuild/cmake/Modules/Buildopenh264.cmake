include(ExternalProject)

# Cisco's H.264 codec, for the encoder: the only H.264 encoder FFmpeg has
# that is not x264, which is GPL. Off unless asked for. Writing H.264 is
# what needs the patent license, which a product turning this on has to
# hold itself: Cisco pays the royalties only for its own binaries,
# downloaded by the user, and this builds from source.
set(openh264_VERSION "2.7.0")
set(openh264_URL "https://github.com/cisco/openh264/archive/refs/tags/v${openh264_VERSION}.tar.gz")
set(openh264_HASH "SHA256=5178775c8b440073e70920e97d195efff721c0e56d0a78cc82c919e95d54924d")

set(openh264_DEPS)
if(TLRENDER_NASM)
    list(APPEND openh264_DEPS NASM)
endif()

# Its own Makefile rather than Meson, which would be one more build tool
# for one library. Static, like the other codecs here; the Makefile builds
# position independent code already. "install-static" builds the static
# library alone and installs it with a pkg-config file that names the C++
# runtime it needs, which is how FFmpeg finds and links it.
if(CMAKE_BUILD_TYPE MATCHES "^Debug$")
    set(openh264_BUILDTYPE Debug)
else()
    set(openh264_BUILDTYPE Release)
endif()
include(ProcessorCount)
ProcessorCount(openh264_BUILD_JOBS)
set(openh264_MAKE_ARGS
    PREFIX=${CMAKE_INSTALL_PREFIX}
    BUILDTYPE=${openh264_BUILDTYPE})
if(TLRENDER_NASM)
    list(APPEND openh264_MAKE_ARGS ASM=${CMAKE_INSTALL_PREFIX}/bin/nasm)
endif()
set(openh264_ENV)
if(APPLE)
    # The Makefile names libstdc++ for a static link, which macOS does not
    # have; its C++ runtime is libc++.
    list(APPEND openh264_MAKE_ARGS STATIC_LDFLAGS=-lc++)
    if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND openh264_MAKE_ARGS ARCH=${CMAKE_OSX_ARCHITECTURES})
    endif()
    # In the environment, not on the command line: a CFLAGS given there
    # would replace the Makefile's own instead of adding to them.
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND openh264_ENV MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    endif()
endif()

if(WIN32)
    if(NOT MSYS_INSTALL_PATH AND DEFINED ENV{MSYS_INSTALL_PATH})
        file(TO_CMAKE_PATH "$ENV{MSYS_INSTALL_PATH}" MSYS_INSTALL_PATH)
    endif()
    find_package(Msys REQUIRED)
    # With MSVC, from MSYS2 as FFmpeg is. The C runtime is the DLL one,
    # FFmpeg's, where the Makefile would compile the static one in: these
    # optimization flags are the Makefile's own, with -MD for its -MT.
    if(openh264_BUILDTYPE STREQUAL "Debug")
        list(APPEND openh264_MAKE_ARGS "CFLAGS_DEBUG='-Od -Oy- -FS -D_DEBUG -MDd'")
    else()
        list(APPEND openh264_MAKE_ARGS "CFLAGS_OPT='-O2 -Ob1 -Oy- -FS -GF -GS -Gy -DNDEBUG -MD'")
    endif()
    list(APPEND openh264_MAKE_ARGS OS=msvc ARCH=x86_64)
    list(JOIN openh264_MAKE_ARGS " " openh264_MAKE_ARGS_TMP)
    set(openh264_MSYS2
        ${MSYS_CMD}
        -use-full-path
        -defterm
        -no-start
        -here)
    set(openh264_BUILD ${openh264_MSYS2} -c
        "pacman -S --needed make nasm --noconfirm && make -j${openh264_BUILD_JOBS} ${openh264_MAKE_ARGS_TMP} openh264.lib")
    set(openh264_INSTALL ${openh264_MSYS2} -c
        "make ${openh264_MAKE_ARGS_TMP} install-static")
else()
    if(BSD)
        set(openh264_MAKE gmake)
    else()
        set(openh264_MAKE make)
    endif()
    set(openh264_BUILD ${CMAKE_COMMAND} -E env ${openh264_ENV}
        ${openh264_MAKE} -j${openh264_BUILD_JOBS} ${openh264_MAKE_ARGS} libopenh264.a)
    set(openh264_INSTALL ${CMAKE_COMMAND} -E env ${openh264_ENV}
        ${openh264_MAKE} ${openh264_MAKE_ARGS} install-static)
endif()

ExternalProject_Add(
    openh264
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openh264
    DEPENDS ${openh264_DEPS}
    URL ${openh264_URL}
    URL_HASH ${openh264_HASH}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${openh264_BUILD}
    INSTALL_COMMAND ${openh264_INSTALL}
    BUILD_IN_SOURCE 1)
