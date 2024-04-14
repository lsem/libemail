#!/usr/bin/env bash

set -e

export SDKROOT=$(xcrun --sdk macosx --show-sdk-path)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

BUILD_DIR_BASE=_build
DEBUG_BUILD_DIR=$BUILD_DIR_BASE/debug
RELEASE_BUILD_DIR=$BUILD_DIR_BASE/release
DEBUG_INSTALL_DIR=$SCRIPT_DIR/$DEBUG_BUILD_DIR/dist
RELEASE_INSTALL_DIR=$SCRIPT_DIR/$RELEASE_BUILD_DIR/dist

cmake -B $DEBUG_BUILD_DIR --install-prefix $DEBUG_INSTALL_DIR -GNinja -DCMAKE_BUILD_TYPE=Debug
cmake --build $DEBUG_BUILD_DIR
cmake --install $DEBUG_BUILD_DIR
python3 patch_installed_libs.py $DEBUG_INSTALL_DIR/lib/libmailer_poc.dylib $DEBUG_INSTALL_DIR/ $BUILD_DIR_BASE/embedded_libs/debug

# Release is disabled for now.
# cmake -B $RELEASE_BUILD_DIR --install-prefix $RELEASE_INSTALL_DIR -GNinja -DCMAKE_BUILD_TYPE=Release
# cmake --build $RELEASE_BUILD_DIR
# cmake --install $RELEASE_BUILD_DIR
# python3 patch_installed_libs.py $RELEASE_INSTALL_DIR/lib/libmailer_poc.dylib $RELEASE_INSTALL_DIR/ $BUILD_DIR_BASE/embedded_libs/release

# mkdir -p $BUILD_DIR_BASE/embedded_libs # 

# TODO: run debug or release depending on selected one.
cp $BUILD_DIR_BASE/embedded_libs/debug/*.dylib _build/embedded_libs/

