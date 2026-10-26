#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${RE1_FFMPEG_SOURCE:?Set RE1_FFMPEG_SOURCE to the FFmpeg n7.1.1 source checkout}"
command -v emcc >/dev/null
root="$PWD"
prefix="$root/build/ffmpeg-wasm"
mkdir -p "$root/build/ffmpeg-objects"
if [ ! -f "$prefix/lib/libavcodec.a" ]; then
  cd "$root/build/ffmpeg-objects"
  emconfigure "$RE1_FFMPEG_SOURCE/configure" --prefix="$prefix" \
    --cc=emcc --cxx=em++ --ar=emar --ranlib=emranlib --nm=emnm \
    --enable-cross-compile --target-os=none --arch=wasm32 --cpu=generic \
    --disable-asm --disable-x86asm --disable-inline-asm --disable-programs \
    --disable-doc --disable-network --disable-pthreads --disable-everything \
    --enable-avformat --enable-avcodec --enable-avutil --enable-swresample \
    --enable-demuxer=avi,mov --enable-decoder=cinepak,pcm_s16le,pcm_u8,adpcm_ms,adpcm_ima_wav,h264,hevc,aac \
    --enable-protocol=file --disable-autodetect --extra-cflags=-O2
  make -j"${RE1_BUILD_JOBS:-4}"
  make install
fi
cd "$root"
emcmake cmake -S . -B build/web -G Ninja -DRE1_FFMPEG_ROOT="$prefix" -DCMAKE_BUILD_TYPE=Release
cmake --build build/web -j"${RE1_BUILD_JOBS:-4}"
cp build/web/residentevil.{js,wasm} android/app/src/main/assets/web/
# Keep mod decoding off the render thread and preserve RGB under transparent
# pixels; canvas/ImageBitmap round-trips premultiply and lose those colours.
webp_source="${RE1_WEBP_SOURCE:-$root/build/dependency-source/libwebp}"
if [ ! -f "$webp_source/CMakeLists.txt" ]; then
  mkdir -p "$webp_source"
  tar -xzf "$root/dependency-sources/libwebp-1.6.0-source.tar.gz" --strip-components=1 -C "$webp_source"
fi
if [ ! -f "$root/build/webp-wasm/libwebpdecoder.a" ]; then
  emcmake cmake -S "$webp_source" -B "$root/build/webp-wasm" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF -DWEBP_USE_THREAD=OFF -DWEBP_ENABLE_SIMD=OFF \
    -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
    -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
    -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
  cmake --build "$root/build/webp-wasm" --target webpdecoder -j"${RE1_BUILD_JOBS:-4}"
fi
em++ -O3 -msimd128 -ffp-contract=off -std=c++17 -I src -I "$webp_source/src" tools/hd/resize.cpp "$root/build/webp-wasm/libwebpdecoder.a" -sUSE_ZLIB=1 -sMODULARIZE=1 -sEXPORT_NAME=RE1HDResize -sENVIRONMENT=worker \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=16777216 -sMAXIMUM_MEMORY=268435456 -sDYNAMIC_EXECUTION=0 \
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_re1_hd_key','_re1_hd_resize','_re1_hd_compress','_re1_mod_decode']" "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']" \
    -o android/app/src/main/assets/web/hd-resize.js
