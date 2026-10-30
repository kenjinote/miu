#!/bin/sh
set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

echo "Building universal macOS libced.a..."
clang++ -std=c++17 -O2 -mmacosx-version-min=13.0 -arch arm64 -arch x86_64 -Iinclude \
  -c include/compact_enc_det/compact_enc_det.cc \
     include/compact_enc_det/compact_enc_det_hint_code.cc \
     include/util/encodings/encodings.cc \
     include/util/languages/languages.cc
libtool -static -o macOS/libced.a \
  compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o
rm -f compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o

echo "Building iOS libced_iOS.a (arm64)..."
xcrun -sdk iphoneos clang++ -std=c++17 -O2 -miphoneos-version-min=16.0 -arch arm64 -Iinclude \
  -c include/compact_enc_det/compact_enc_det.cc \
     include/compact_enc_det/compact_enc_det_hint_code.cc \
     include/util/encodings/encodings.cc \
     include/util/languages/languages.cc
libtool -static -o macOS/libced_iOS.a \
  compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o
rm -f compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o
