#!/bin/sh
# Assemble DJ tester packages under dist/dj-testers/
# Usage: package-dj-testers.sh [path-to-ci-artifacts-or-dist]
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="0.1.16-ttns-pre.6"
OUT="$ROOT/dist/dj-testers"
PREFIX="TTNS-Deck-${VER}"
SEARCH="${1:-$ROOT/dist}"
STAGE="$ROOT/dist/.release-staging"

cd "$ROOT"

# Local macOS-only build when no CI artifacts present
if [ "$SEARCH" = "$ROOT/dist" ] && [ ! -f "$ROOT/dist/macos/ttns-deck-arm64-macos.zip" ]; then
    if [ ! -x "$ROOT/src/butt" ]; then
        echo "Building binary first..."
        autoreconf -fi
        ./configure -q
        make -C src -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
    fi
    "$ROOT/scripts/build-macos-app.sh"
fi

"$ROOT/scripts/stage-release-packages.sh" "$STAGE" "$SEARCH"

mkdir -p "$OUT"
rm -f "$OUT"/TTNS-Deck-*.zip "$OUT"/TTNS-Deck-*.tar.gz 2>/dev/null || true

cp "$STAGE/ttns-deck-arm64-macos.zip"     "$OUT/${PREFIX}-macos-arm64.zip"
cp "$STAGE/ttns-deck-x86_64-macos.zip"    "$OUT/${PREFIX}-macos-x64.zip"
cp "$STAGE/ttns-deck-linux-x86_64.tar.gz" "$OUT/${PREFIX}-linux-x64.tar.gz"
cp "$STAGE/ttns-deck-win64.zip"           "$OUT/${PREFIX}-windows-x64.zip"

cp "$ROOT/docs/TESTER_FEEDBACK.md" "$OUT/README-TESTERS.txt"

cat > "$OUT/WHATS-IN-HERE.txt" <<EOF
TTNS Deck preliminary build: ${VER}

Send DJs the zip/tar.gz for their platform:

  ${PREFIX}-macos-arm64.zip     Apple Silicon Mac
  ${PREFIX}-macos-x64.zip       Intel Mac
  ${PREFIX}-linux-x64.tar.gz    Linux x86_64
  ${PREFIX}-windows-x64.zip     Windows 10+

Each package includes legal/ (GPL + third-party license texts).

Also read README-TESTERS.txt for log file locations and feedback instructions.

Built: $(date -u +"%Y-%m-%dT%H:%MZ")
EOF

echo ""
echo "DJ tester packages in: $OUT"
ls -lh "$OUT"/${PREFIX}-* "$OUT"/README-TESTERS.txt "$OUT"/WHATS-IN-HERE.txt
