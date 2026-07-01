#!/bin/sh
# Assemble DJ tester packages under dist/dj-testers/
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="0.1.16-ttns-pre.3"
OUT="$ROOT/dist/dj-testers"
PREFIX="TTNS-Deck-${VER}"

cd "$ROOT"

if [ ! -x "$ROOT/src/butt" ]; then
    echo "Building binary first..."
    autoreconf -fi
    ./configure -q
    make -C src -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
fi

"$ROOT/scripts/build-macos-app.sh"

mkdir -p "$OUT"
rm -f "$OUT"/${PREFIX}-*.zip "$OUT"/${PREFIX}-*.tar.gz 2>/dev/null || true

cp "$ROOT/dist/macos/ttns-deck-arm64-macos.zip" \
   "$OUT/${PREFIX}-macos-arm64.zip"

# Intel mac zip from CI if present, else note in README
if [ -f "$OUT/ttns-deck-macos-x64/ttns-deck-x86_64-macos.zip" ]; then
    cp "$OUT/ttns-deck-macos-x64/ttns-deck-x86_64-macos.zip" \
       "$OUT/${PREFIX}-macos-x64.zip"
fi

if [ -f "$OUT/ttns-deck-linux-x64/ttns-deck-linux-x86_64.tar.gz" ]; then
    cp "$OUT/ttns-deck-linux-x64/ttns-deck-linux-x86_64.tar.gz" \
       "$OUT/${PREFIX}-linux-x64.tar.gz"
fi

if [ -f "$OUT/ttns-deck-windows-x64/ttns-deck-win64.zip" ]; then
    cp "$OUT/ttns-deck-windows-x64/ttns-deck-win64.zip" \
       "$OUT/${PREFIX}-windows-x64.zip"
fi

cp "$ROOT/docs/TESTER_FEEDBACK.md" "$OUT/README-TESTERS.txt"

cat > "$OUT/WHATS-IN-HERE.txt" <<EOF
TTNS Deck preliminary build: ${VER}

Send DJs the zip/tar.gz for their platform:

  ${PREFIX}-macos-arm64.zip     Apple Silicon Mac
  ${PREFIX}-macos-x64.zip       Intel Mac (if present)
  ${PREFIX}-linux-x64.tar.gz    Linux x86_64 (if present)
  ${PREFIX}-windows-x64.zip     Windows 10+ (if present)

Also read README-TESTERS.txt for log file locations and feedback instructions.

Built: $(date -u +"%Y-%m-%dT%H:%MZ")
EOF

echo ""
echo "DJ tester packages in: $OUT"
ls -lh "$OUT"/${PREFIX}-* "$OUT"/README-TESTERS.txt "$OUT"/WHATS-IN-HERE.txt 2>/dev/null || ls -lh "$OUT"
