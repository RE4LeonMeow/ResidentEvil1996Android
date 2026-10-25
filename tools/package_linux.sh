#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# package_linux.sh - build a self-contained Linux x86-32 bundle.
#
# The game is a 32-bit ELF dynamically linked against SDL2 and ffmpeg, and no
# two distributions agree on the ffmpeg sonames (ffmpeg 6 = libavcodec.so.60 on
# the Ubuntu build host, ffmpeg 9 = libavcodec.so.62 on Arch/CachyOS), so the
# libraries travel with the binary and the launcher points LD_LIBRARY_PATH at
# them. That is also the only way onto a Steam Deck: SteamOS has a read-only
# rootfs and a frozen package repo, so the libraries cannot be installed there.
#
# Usage (from the repo root, on the Linux build host):
#   bash tools/package_linux.sh [--no-build] [--with-assets] [--out DIR]
#
# Output: <out>/residentevil-<version>-linux-x86/
#   residentevil          the game (ELF32 i386)
#   residentevil.sh       launcher (cd's here, sets LD_LIBRARY_PATH)
#   lib/                  bundled 32-bit libraries
#   README-LINUX.txt      how to run it and where the game data goes
#
# Game data is NOT bundled by default - it is the retail game's, the same
# policy as the Windows release. --with-assets copies assets/ for a personal
# deployment (about 1.2 GB).
# ---------------------------------------------------------------------------
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO/build/linux"
BIN="$BUILD_DIR/residentevil"
OUT_ROOT="$REPO/dist"
DO_BUILD=1
WITH_ASSETS=0

usage() {
    sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build)    DO_BUILD=0 ;;
        --with-assets) WITH_ASSETS=1 ;;
        --out)         OUT_ROOT="$2"; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if [ "$DO_BUILD" -eq 1 ]; then
    echo "==> configuring + building"
    cmake -S "$REPO" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

[ -f "$BIN" ] || { echo "error: $BIN missing; run without --no-build" >&2; exit 1; }

# The bundle is only meaningful for the 32-bit target: a 64-bit binary would
# already find everything it needs through the normal paths.
elfclass="$(od -An -tu1 -j4 -N1 "$BIN" | tr -d ' ')"
elfmachine="$(od -An -tu2 -j18 -N1 "$BIN" | tr -d ' ')"
if [ "$elfclass" != "1" ] || [ "$elfmachine" != "3" ]; then
    echo "error: $BIN is not ELF32 i386 (class=$elfclass machine=$elfmachine)" >&2
    exit 1
fi

# Note for both of these: no `| head -1`. Under `set -o pipefail` a consumer
# that exits early (head, grep -q) SIGPIPEs its producer, the pipeline reports
# 141, and errexit kills the script silently - mid-run, with no message.
VERSION="$(grep -m1 'GAME_VERSION_STRING' "$REPO/src/Version.h" | sed 's/.*"\([^"]*\)".*/\1/')"
[ -n "$VERSION" ] || VERSION="unknown"
GLIBC="$(ldd --version | awk 'NR==1 {print $NF}')"

OUT="$OUT_ROOT/residentevil-$VERSION-linux-x86"
echo "==> staging $OUT"
rm -rf "$OUT"
mkdir -p "$OUT/lib"

cp "$BIN" "$OUT/residentevil"
chmod +x "$OUT/residentevil"

# ---------------------------------------------------------------------------
# Library closure
#
# Only two groups stay the host's. LD_LIBRARY_PATH is process-global, so a
# bundled libGL/libdrm/libgbm would also be picked up by the system's GL
# driver - and that stack has to match the running kernel and GPU. The glibc
# family is the loader itself and can never be bundled.
#
# Everything else travels, including the window-system and sound clients
# (libX11, libxcb, libwayland, libxkbcommon, libdecor, libasound, libpulse):
# they talk to the host's display and sound daemons over stable protocols, and
# bundling them is what makes the bundle independent of the target's 32-bit
# package set. Leaving them out only moves the failure to the target machine.
# ---------------------------------------------------------------------------
HOST_OWNED='^(ld-linux|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|libresolv\.so|libutil\.so|libanl\.so|libnsl\.so|libnss_|libthread_db|libcrypt\.so|libGL|libEGL|libGLX|libOpenGL|libgbm|libdrm|libvulkan|libgallium|libglapi)'

mapfile -t CLOSURE < <(ldd "$BIN" | awk '/=>/ && $3 != "" {print $3}' | sort -u)

kept=0
skipped=0
for lib in "${CLOSURE[@]}"; do
    base="$(basename "$lib")"
    if [[ "$base" =~ $HOST_OWNED ]]; then
        skipped=$((skipped + 1))
        continue
    fi
    # -L: several of these are symlinks to the real versioned object.
    cp -L "$lib" "$OUT/lib/$base"
    kept=$((kept + 1))
done
echo "==> bundled $kept libraries, left $skipped to the host"

# Fail here rather than on the target machine: every dependency that is not
# host-owned has to resolve from lib/, and the host-owned ones must exist on
# this machine (they are what the target is expected to provide). grep reads to
# the end rather than exiting on the first match - see the note above.
missing="$(LD_LIBRARY_PATH="$OUT/lib" ldd "$OUT/residentevil" | grep "not found" || true)"
if [ -n "$missing" ]; then
    echo "error: unresolved dependencies after bundling:" >&2
    echo "$missing" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Launcher
# ---------------------------------------------------------------------------
cat > "$OUT/residentevil.sh" <<'LAUNCHER'
#!/usr/bin/env bash
# Launcher for the bundled build: puts lib/ on LD_LIBRARY_PATH so the bundled
# 32-bit SDL2 and ffmpeg are used instead of the distribution's. It also cd's
# here, which is where the logs and config.ini land (the asset and save folders
# are anchored to the binary's own directory by config.ini, not to the cwd).
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$HERE" || exit 1
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/residentevil" "$@"
LAUNCHER
chmod +x "$OUT/residentevil.sh"

cat > "$OUT/README-LINUX.txt" <<README
Resident Evil PC decomp - Linux x86 (32-bit), version $VERSION
============================================================

Run:
    ./residentevil.sh

The launcher cd's into this directory and puts lib/ on LD_LIBRARY_PATH, so the
bundled 32-bit libraries are used instead of whatever this distribution happens
to have (or does not have) - including SDL2, ffmpeg, the X11/Wayland clients and
the ALSA/PulseAudio clients.

The GL driver stack (libGL, libEGL, libdrm, libgbm, libvulkan) is deliberately
NOT bundled: it has to match the running kernel and GPU, so the system provides
it. On Arch/CachyOS that means lib32-mesa (SteamOS and any 32-bit-capable
desktop already have it).

Game data is not included. Put your Resident Evil (1996 PC) data tree next to
this file as USA/ (and JPN/ for the Japanese PC version):

    USA/...
    JPN/...        optional

That is the default: [Assets] Path in config.ini is empty, meaning "the folder
this executable is in", and saves go to save/ beside it. config.ini is created
here on first run; its [Assets] Path and [Save] Path settings point the data
and saves somewhere else instead (relative paths resolve from this folder).

Requirements: a 64-bit distribution with 32-bit support (multilib), and glibc
$GLIBC or newer. On a Steam Deck, add residentevil.sh as a non-Steam game
(Desktop Mode -> right-click Steam -> Add a Non-Steam Game) rather than
launching it through the Steam Linux Runtime, whose older glibc the build does
not target.

Diagnostics go to stderr; RE1_DEBUGLOG=1 also appends them to re1_debug.log.
A fatal signal writes a symbolized crash.log in this directory.
README

if [ "$WITH_ASSETS" -eq 1 ]; then
    echo "==> copying assets (this takes a moment)"
    mkdir -p "$OUT/assets"
    cp -r "$REPO/assets/USA" "$OUT/assets/"
    [ -d "$REPO/assets/JPN" ] && cp -r "$REPO/assets/JPN" "$OUT/assets/"
    [ -d "$REPO/assets/SAVE" ] && cp -r "$REPO/assets/SAVE" "$OUT/assets/"
fi

echo "==> $OUT"
du -sh "$OUT"
echo "    $(find "$OUT/lib" -type f | wc -l) libraries, $(du -sh "$OUT/lib" | cut -f1)"
