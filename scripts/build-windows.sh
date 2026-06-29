# Build TTNS Deck on Windows 10+ (MSYS2 / MinGW64).
# Run inside "MSYS2 MinGW x64" terminal:
#   pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-fltk \
#     mingw-w64-x86_64-portaudio mingw-w64-x86_64-lame mingw-w64-x86_64-libvorbis \
#     mingw-w64-x86_64-flac mingw-w64-x86_64-opus mingw-w64-x86_64-libsamplerate \
#     mingw-w64-x86_64-fdk-aac autoconf automake libtool pkg-config git
#
# Then from repo root:
#   bash scripts/build-windows.sh

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist/windows"
STAGE="$DIST/ttns-deck-win64"
ZIP="$DIST/ttns-deck-win64.zip"

cd "$ROOT"
[ -f "$ROOT/scripts/generate-icons.sh" ] && bash "$ROOT/scripts/generate-icons.sh" 2>/dev/null || true

autoreconf -fi
./configure -q
make -j"$(nproc 2>/dev/null || echo 4)"

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/data" "$STAGE/assets"

cp "$ROOT/src/butt.exe" "$STAGE/bin/ttns-deck.exe" 2>/dev/null || cp "$ROOT/src/butt" "$STAGE/bin/ttns-deck.exe"
cp "$ROOT/data/ttns-zones.json" "$STAGE/data/"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/assets/"
cp "$ROOT/docs/TTNS_DJ_GUIDE.md" "$STAGE/README.txt"

# Bundle MinGW runtime DLLs next to exe (required on target PCs without MSYS2).
for dll in \
    libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
    libfltk-1.3.dll libfltk_images-1.3.dll libportaudio.dll \
    libmp3lame-0.dll libvorbis-0.dll libvorbisenc-2.dll libvorbisfile-3.dll libogg-0.dll \
    libopus-0.dll libFLAC-13.dll libsamplerate-0.dll libfdk-aac-2.dll \
    zlib1.dll libpng16-16.dll libjpeg-8.dll
do
    for dir in /mingw64/bin "$MINGW_PREFIX/bin"; do
        [ -f "$dir/$dll" ] && cp "$dir/$dll" "$STAGE/bin/" && break
    done
done

cat > "$STAGE/Run TTNS Deck.bat" <<'EOF'
@echo off
cd /d "%~dp0"
bin\ttns-deck.exe %*
EOF

mkdir -p "$DIST"
rm -f "$ZIP"
(cd "$DIST" && powershell -Command "Compress-Archive -Path 'ttns-deck-win64' -DestinationPath 'ttns-deck-win64.zip' -Force" 2>/dev/null) \
    || (cd "$DIST" && zip -r ttns-deck-win64.zip ttns-deck-win64)

echo "Built: $STAGE"
echo "Zip:   $ZIP"
