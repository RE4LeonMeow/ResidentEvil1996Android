#!/usr/bin/env bash
# compile_linux.sh - Phase 1 gate: every shared translation unit compiles on
# Linux x86-32. Compile-only; nothing links yet (docs/LINUX_PORT.md).
#
# Run inside WSL/Linux from the repo root:
#   bash tests/compile_linux.sh
set -u

OUT=${OUT:-obj/linux}
CXX=${CXX:-g++}
FLAGS="-m32 -std=c++17 -Isrc -c -w"

# Windows-only TUs are excluded here; each has its own phase (backends, MCI,
# registry/dbghelp, DXGI). See docs/LINUX_PORT.md section 6.1.
SRC=$(ls src/game/*.cpp src/game/entities/*.cpp \
         src/system/AssetPath.cpp src/system/AudioFile.cpp src/system/ConfigFile.cpp \
         src/marni/MarniBits.cpp src/marni/PSXTexture.cpp \
         src/marni/Marni3DObject.cpp src/marni/MarniSystem.cpp \
         src/video/VideoPlayback.cpp \
         src/Globals.cpp 2>/dev/null)

mkdir -p "$OUT"
fail=0
total=0
for f in $SRC; do
    total=$((total + 1))
    obj="$OUT/$(echo "$f" | tr '/' '_').o"
    # TaskScheduler needs -fomit-frame-pointer: the resume path switches ESP
    # only, so an EBP frame in a task would restore the scheduler's frame.
    extra=""
    case "$f" in
        src/game/TaskScheduler.cpp) extra="-fomit-frame-pointer" ;;
    esac
    if ! $CXX $FLAGS $extra "$f" -o "$obj" 2>"$OUT/err.txt"; then
        fail=$((fail + 1))
        echo "FAIL $f"
        head -12 "$OUT/err.txt" | sed 's/^/     /'
    fi
done

echo "---------------------------------------------"
echo "compiled $((total - fail))/$total TUs"
[ "$fail" -eq 0 ] || exit 1
