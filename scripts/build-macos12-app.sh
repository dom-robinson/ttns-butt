#!/bin/sh
# Build TTNS Deck + Remote for macOS 12 (Monterey) Apple Silicon testers.
# Rebuilds third-party libs from source so Homebrew's macOS 26 bottles are not used.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIN="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
ARCH="$(uname -m)"
PREFIX="${TTNS_DEP_PREFIX:-$ROOT/deps/macos${MIN%%.*}-$ARCH}"
SDKROOT="${SDKROOT:-$(xcrun --show-sdk-path)}"
VER="$(tr -d '[:space:]' < "$ROOT/packaging/VERSION")"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ "$ARCH" != "arm64" ]; then
    echo "This Monterey rebuild targets Apple Silicon (arm64). Host is $ARCH." >&2
    exit 1
fi

chmod +x "$ROOT/scripts/build-macos12-deps.sh" "$ROOT/scripts/build-macos-app.sh"
TTNS_DEP_PREFIX="$PREFIX" MACOSX_DEPLOYMENT_TARGET="$MIN" \
    "$ROOT/scripts/build-macos12-deps.sh"

export TTNS_DEP_PREFIX="$PREFIX"
export MACOSX_DEPLOYMENT_TARGET="$MIN"
export TTNS_MACOS_MIN_OS="$MIN"
export TTNS_DECK_ZIP_STEM="ttns-deck-${ARCH}-macos12"
export TTNS_REMOTE_ZIP_STEM="ttns-remote-${ARCH}-macos12"
export TTNS_MACOS_DMG_LABEL="arm64-monterey12"
export SDKROOT
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export PATH="$PREFIX/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export CPPFLAGS="-I$PREFIX/include -mmacosx-version-min=$MIN -isysroot $SDKROOT"
export CFLAGS="-O2 -mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT"
export CXXFLAGS="-O2 -mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT -stdlib=libc++"
export OBJCXXFLAGS="$CXXFLAGS"
export LDFLAGS="-L$PREFIX/lib -mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT"
# Avoid gnulib rpl_malloc when the SDK malloc(0) probe fails for older min OS.
export ac_cv_func_malloc_0_nonnull=yes
export ac_cv_func_realloc_0_nonnull=yes

cd "$ROOT"
autoreconf -fi
./configure -q
make -C src clean
rm -f src/butt src/ttns_remote
make -C src -j"$JOBS"

"$ROOT/scripts/build-macos-app.sh"

# Stage clearly named tester packages (do not confuse with the current-OS CI build).
mkdir -p "$ROOT/dist/dj-testers"
DECK_DMG="$ROOT/dist/macos/TTNS-Deck-${VER}-macos-arm64-monterey12.dmg"
REMOTE_DMG="$ROOT/dist/macos/TTNS-Remote-${VER}-macos-arm64-monterey12.dmg"
[ -f "$DECK_DMG" ] && cp "$DECK_DMG" "$ROOT/dist/dj-testers/"
[ -f "$REMOTE_DMG" ] && cp "$REMOTE_DMG" "$ROOT/dist/dj-testers/"

python3 - <<PY
import os, re, subprocess, sys
root = "$ROOT"
app = os.path.join(root, "dist/macos/TTNS Deck.app")
bin_path = os.path.join(app, "Contents/MacOS/ttns-deck-bin")
fw = os.path.join(app, "Contents/Frameworks")

def minos(path):
    out = subprocess.check_output(["otool", "-l", path], text=True, errors="replace")
    vals = re.findall(r"minos\\s+(\\S+)", out)
    vals += re.findall(r"LC_VERSION_MIN_MACOSX[\\s\\S]*?version\\s+(\\S+)", out)
    return vals[-1] if vals else "?"

bad = []
for path in [bin_path] + [
    os.path.join(fw, n) for n in os.listdir(fw) if n.endswith(".dylib")
]:
    v = minos(path)
    major = int(v.split(".")[0]) if v[0].isdigit() else 99
    if major > 12:
        bad.append(f"{os.path.basename(path)} minos={v}")
if bad:
    print("ERROR: binaries still require newer than macOS 12:", file=sys.stderr)
    for b in bad:
        print(" ", b, file=sys.stderr)
    sys.exit(1)
print("Verified macOS 12 deployment target on Deck binary + bundled dylibs")
PY

echo ""
echo "Monterey Apple Silicon — prefer the .dmg (send the file, not the .app):"
if [ -f "$DECK_DMG" ]; then
    echo "  $DECK_DMG"
fi
if [ -f "$REMOTE_DMG" ]; then
    echo "  $REMOTE_DMG"
fi
