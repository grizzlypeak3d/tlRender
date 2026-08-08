#!/bin/sh

# Build the dependencies and then tlRender, into directories beside the
# current one. What to build is in etc/Config/*.cmake rather than here: this
# script is only the part that differs between platforms.

set -e
set -x

SOURCE_DIR=$1
BUILD_TYPE=$2
CONFIG=${3:-default}
CONFIG_FILE=$SOURCE_DIR/etc/Config/$CONFIG.cmake

# Build with every core unless told otherwise; "cmake --build" reads this.
if [ -z "$CMAKE_BUILD_PARALLEL_LEVEL" ]; then
    CMAKE_BUILD_PARALLEL_LEVEL=$(sysctl -n hw.ncpu)
    export CMAKE_BUILD_PARALLEL_LEVEL
fi

git -C $SOURCE_DIR submodule update --init --recursive

for STAGE in "deps/ftk/etc/SuperBuild ftk" "etc/SuperBuild tl"; do
    set -- $STAGE
    cmake \
        -S $SOURCE_DIR/$1 \
        -B $2-$BUILD_TYPE \
        -C $CONFIG_FILE \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_INSTALL_PREFIX=$PWD/install-$BUILD_TYPE \
        -DCMAKE_PREFIX_PATH=$PWD/install-$BUILD_TYPE
    cmake --build $2-$BUILD_TYPE --config $BUILD_TYPE
done

cmake \
    -S $SOURCE_DIR \
    -B build-$BUILD_TYPE \
    -C $CONFIG_FILE \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=$PWD/install-$BUILD_TYPE \
    -DCMAKE_PREFIX_PATH=$PWD/install-$BUILD_TYPE
cmake --build build-$BUILD_TYPE --config $BUILD_TYPE

# The install directory is how everything downstream finds what was built:
# the tests import ftkPy from there, and packaging reads it.
cmake --build build-$BUILD_TYPE --config $BUILD_TYPE --target install
