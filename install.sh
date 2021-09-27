#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$ROOT/build"
CUSTOM_DIR="$ROOT/4coder_base/custom"
INSTALL_DIR=~/apps/4coder

mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/themes"
mkdir -p "$BUILD_DIR/fonts"

pushd "$BUILD_DIR"
"$CUSTOM_DIR/bin/buildsuper_x64-linux.sh" "$ROOT/custom.cpp" release
cp -r "$ROOT/themes" "$INSTALL_DIR/"
cp -r "$ROOT/fonts" "$INSTALL_DIR/"
cp -r "$ROOT/4coder_base/fonts" "$INSTALL_DIR/"
cp -r "$ROOT/4coder_base/themes" "$INSTALL_DIR/"
cp "$ROOT/config.4coder" "$INSTALL_DIR/"
cp "$ROOT/config.4coder" "$INSTALL_DIR/"

cp custom_4coder.so $INSTALL_DIR
cp 4ed_app.so $INSTALL_DIR
cp 4ed $INSTALL_DIR

popd

