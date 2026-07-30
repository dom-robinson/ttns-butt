#!/bin/sh
# Assemble DJ tester packages under dist/dj-testers/
# Usage: package-dj-testers.sh [path-to-ci-artifacts-or-dist]
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="0.1.16-ttns-remote-dev.2"
OUT="$ROOT/dist/dj-testers"
PREFIX="TTNS-Deck-${VER}"
REMOTE_PREFIX="TTNS-Remote-${VER}"
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
rm -f "$OUT"/TTNS-Deck-*.zip "$OUT"/TTNS-Deck-*.tar.gz \
      "$OUT"/TTNS-Remote-*.zip 2>/dev/null || true

cp "$STAGE/ttns-deck-arm64-macos.zip"     "$OUT/${PREFIX}-macos-arm64.zip"
cp "$STAGE/ttns-deck-x86_64-macos.zip"    "$OUT/${PREFIX}-macos-x64.zip"
cp "$STAGE/ttns-deck-linux-x86_64.tar.gz" "$OUT/${PREFIX}-linux-x64.tar.gz"
cp "$STAGE/ttns-deck-win64.zip"           "$OUT/${PREFIX}-windows-x64.zip"

# Optional standalone Remote zips (macOS CI / local). Linux/Windows ship remote inside Deck package.
for arch in arm64 x86_64; do
    label="$arch"
    [ "$arch" = "x86_64" ] && label="x64"
    f="$(find "$SEARCH" -type f -name "ttns-remote-${arch}-macos.zip" 2>/dev/null | head -1)"
    if [ -n "$f" ]; then
        cp "$f" "$OUT/${REMOTE_PREFIX}-macos-${label}.zip"
        echo "Staged remote macOS ${label}"
    fi
done

cp "$ROOT/docs/TESTER_FEEDBACK.md" "$OUT/README-TESTERS.txt"
[ -f "$ROOT/docs/USER_GUIDE.md" ] && cp "$ROOT/docs/USER_GUIDE.md" "$OUT/USER_GUIDE.md"

cat > "$OUT/WHATS-IN-HERE.txt" <<EOF
TTNS Deck + Remote crew test build: ${VER}

Send each person the zip/tar.gz for their platform:

  ${PREFIX}-macos-arm64.zip     Apple Silicon Mac (Deck.app; includes ttns-remote in bundle)
  ${PREFIX}-macos-x64.zip       Intel Mac
  ${PREFIX}-linux-x64.tar.gz    Linux x86_64 (run-ttns-deck.sh / run-ttns-remote.sh)
  ${PREFIX}-windows-x64.zip     Windows 10+ (Run TTNS Deck.bat; bin/ttns-remote.exe)

Optional standalone Remote (macOS), if present:

  ${REMOTE_PREFIX}-macos-arm64.zip
  ${REMOTE_PREFIX}-macos-x64.zip

Docs in this folder:
  README-TESTERS.txt   short tester notes + log paths
  USER_GUIDE.md        full Deck + Remote guide (add screenshots later)

Each package includes legal/ (GPL + third-party license texts).

Built: $(date -u +"%Y-%m-%dT%H:%MZ")
EOF

echo ""
echo "DJ tester packages in: $OUT"
ls -lh "$OUT"
