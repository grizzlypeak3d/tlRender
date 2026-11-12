#!/bin/sh

# Build the web player with Emscripten: feather-tk's dependencies and
# tlRender's, FFmpeg's three utility libraries, and then this tree with
# the player example. Run from an empty directory with emcc on the path:
#
#     sh path/to/tlRender/etc/Web/build.sh path/to/tlRender
#
# The result is build/examples/player; serve that directory with the
# COOP/COEP headers (or let coi-serviceworker.js add them) and open it.
#
# A stage whose libraries are already installed is skipped, so a kept
# install directory -- the continuous integration cache -- makes a run
# cost only this tree's build.
#
# Everything is compiled with threads, the dependencies included: a
# library without them cannot be linked into a program with them.

set -e

SOURCE_DIR=$(cd "$1" && pwd)
PREFIX=$PWD/install
JOBS=${CMAKE_BUILD_PARALLEL_LEVEL:-8}
CFLAGS="-pthread -sSUPPORT_LONGJMP=wasm"
CXXFLAGS="-pthread -fwasm-exceptions"

# The resource compiler runs on the build machine, not in the browser.
if [ ! -x ftk-resource ]; then
    c++ -O2 -std=c++17 \
        $SOURCE_DIR/deps/ftk/bin/ftk-resource/main.cpp \
        $SOURCE_DIR/deps/ftk/bin/ftk-resource/App.cpp \
        -lz -o ftk-resource
fi

if [ ! -f $PREFIX/lib/liblunasvg.a ]; then
    emcmake cmake -S $SOURCE_DIR/deps/ftk/etc/SuperBuild -B ftk-superbuild \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        "-DCMAKE_C_FLAGS=$CFLAGS" \
        "-DCMAKE_CXX_FLAGS=$CXXFLAGS" \
        -Dftk_SDL2=OFF \
        -Dftk_SDL3=OFF \
        -Dftk_nfd=OFF \
        -Dftk_glslang=OFF
    cmake --build ftk-superbuild -j $JOBS
fi

# Only what the core uses: avutil, swresample and swscale, built as
# FFmpeg's own build system wants, since the super build's recipe is for
# the whole of FFmpeg on a desktop.
if [ ! -f $PREFIX/lib/libswscale.a ]; then
    FFMPEG_VERSION=n9.0.2
    if [ ! -d FFmpeg-$FFMPEG_VERSION ]; then
        curl -sL -o ffmpeg.tar.gz \
            https://github.com/FFmpeg/FFmpeg/archive/refs/tags/$FFMPEG_VERSION.tar.gz
        tar xzf ffmpeg.tar.gz
    fi
    (
        cd FFmpeg-$FFMPEG_VERSION
        emconfigure ./configure \
            --prefix=$PREFIX \
            --cc=emcc --cxx=em++ --ar=emar --ranlib=emranlib \
            --nm=$(em-config LLVM_ROOT)/llvm-nm \
            --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
            --disable-all --enable-avutil --enable-swresample --enable-swscale \
            --disable-programs --disable-doc --disable-network \
            --disable-asm --disable-x86asm --disable-inline-asm \
            --disable-stripping --disable-debug --disable-autodetect \
            --enable-pic \
            "--extra-cflags=-pthread -O2 -fPIC" \
            --extra-ldflags=-pthread
        emmake make -j $JOBS install
    )
fi

if [ ! -f $PREFIX/lib/libopentimelineio.a ]; then
    emcmake cmake -S $SOURCE_DIR/etc/SuperBuild -B superbuild \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DCMAKE_PREFIX_PATH=$PREFIX \
        "-DCMAKE_C_FLAGS=$CFLAGS" \
        "-DCMAKE_CXX_FLAGS=$CXXFLAGS" \
        -DTLRENDER_OCIO=OFF \
        -DTLRENDER_JPEG=OFF \
        -DTLRENDER_TIFF=OFF \
        -DTLRENDER_EXR=OFF \
        -DTLRENDER_AOM=OFF \
        -DTLRENDER_SVTAV1=OFF \
        -DTLRENDER_OPENAPV=OFF \
        -DTLRENDER_FFMPEG=OFF \
        -DTLRENDER_NASM=OFF \
        -DTLRENDER_SUBPROCESS=OFF \
        -DTLRENDER_OIIO=OFF \
        -DTLRENDER_LIBRAW=OFF \
        -DTLRENDER_USD=OFF \
        -DTLRENDER_IGNORE_PREFIX_PATH=
    cmake --build superbuild -j $JOBS
fi

emcmake cmake -S $SOURCE_DIR -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_PREFIX_PATH=$PREFIX \
    -DCMAKE_FIND_ROOT_PATH=$PREFIX \
    -Dftk_API=GLES_3 \
    -Dftk_RESOURCE=$PWD/ftk-resource \
    -Dftk_TESTS=OFF \
    -Dftk_EXAMPLES=OFF \
    -Dftk_IGNORE_PREFIX_PATH= \
    -DTLRENDER_IGNORE_PREFIX_PATH= \
    -DTLRENDER_OCIO=OFF \
    -DTLRENDER_EXR=OFF \
    -DTLRENDER_FFMPEG=ON \
    -DTLRENDER_FFMPEG_PLUGIN=OFF \
    -DTLRENDER_FFMPEG_PREFIX=$PREFIX \
    -DTLRENDER_OIIO=OFF \
    -DTLRENDER_USD=OFF \
    -DTLRENDER_PROGRAMS=OFF \
    -DTLRENDER_EXAMPLES=ON \
    -DTLRENDER_TESTS=OFF
cmake --build build -j $JOBS --target player
