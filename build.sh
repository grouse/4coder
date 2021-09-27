#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$ROOT/build"
CUSTOM_DIR="$ROOT/4coder_base/custom"

mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/themes"
mkdir -p "$BUILD_DIR/fonts"
mkdir -p "$BUILD_DIR/lexer_gen"

pushd "$BUILD_DIR"
"$CUSTOM_DIR/bin/build_one_time.sh" "$ROOT/languages/4coder_cpp_lexer_gen.cpp" "$BUILD_DIR/lexer_gen"
"$BUILD_DIR/lexer_gen/one_time"
"$CUSTOM_DIR/bin/buildsuper_x64-linux.sh" "$ROOT/custom.cpp" debug

cp -r "$ROOT/themes" "$BUILD_DIR/"
cp -r "$ROOT/fonts" "$BUILD_DIR/"
cp "$ROOT/config.4coder" "$BUILD_DIR/"

cp -r "$ROOT/4coder_base/themes" "$BUILD_DIR/"
cp -r "$ROOT/4coder_base/fonts" "$BUILD_DIR/"

popd

