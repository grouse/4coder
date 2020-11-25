#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$ROOT/build"
CUSTOM_DIR="$ROOT/custom"
INSTALL_DIR=~/apps/4coder

mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/themes"
mkdir -p "$BUILD_DIR/fonts"

pushd "$BUILD_DIR"
"$CUSTOM_DIR/bin/buildsuper_x64-linux.sh" "$ROOT/custom.cpp" release
cp -r "$ROOT/themes" "$BUILD_DIR/themes"
cp -r "$ROOT/fonts" "$BUILD_DIR/fonts"
cp "$ROOT/config.4coder" "$BUILD_DIR/"

cp custom_4coder.so $INSTALL_DIR
cp config.4coder $INSTALL_DIR
cp -r themes/* $INSTALL_DIR/themes
cp -r fonts/* $INSTALL_DIR/fonts

popd

