#!/bin/sh
# Assemble DJ tester packages under dist/dj-testers/
# Usage: package-dj-testers.sh [path-to-ci-artifacts-or-dist]
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(tr -d '[:space:]' < "$ROOT/VERSION")"
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
      "$OUT"/TTNS-Deck-*.dmg "$OUT"/TTNS-Deck-*-setup.exe \
      "$OUT"/TTNS-Remote-*.zip 2>/dev/null || true

cp "$STAGE/ttns-deck-arm64-macos.zip"     "$OUT/${PREFIX}-macos-arm64.zip"
cp "$STAGE/ttns-deck-x86_64-macos.zip"    "$OUT/${PREFIX}-macos-x64.zip"
cp "$STAGE/ttns-deck-linux-x86_64.tar.gz" "$OUT/${PREFIX}-linux-x64.tar.gz"
cp "$STAGE/ttns-deck-win64.zip"           "$OUT/${PREFIX}-windows-x64.zip"

for f in "$STAGE"/TTNS-Deck-*.dmg "$STAGE"/TTNS-Deck-*-setup.exe; do
    [ -f "$f" ] || continue
    cp "$f" "$OUT/"
done

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
[ -f "$ROOT/docs/MACOS_GATEKEEPER.md" ] && cp "$ROOT/docs/MACOS_GATEKEEPER.md" "$OUT/MACOS_GATEKEEPER.md"
cp "$ROOT/scripts/macos-clear-quarantine.sh" "$OUT/macos-clear-quarantine.sh"
chmod +x "$OUT/macos-clear-quarantine.sh"

cat > "$OUT/WHATS-IN-HERE.txt" <<EOF
TTNS Deck + Remote ${VER}

Preferred (double-click) packages — send the file itself, not an extracted .app:

  ${PREFIX}-macos-arm64.dmg     Apple Silicon — open, drag TTNS Deck to Applications
  ${PREFIX}-macos-x64.dmg       Intel Mac
  ${PREFIX}-windows-x64-setup.exe  Windows 10+ installer (Start Menu shortcuts)
  ${PREFIX}-linux-x64.tar.gz    Linux x86_64

Portable fallbacks:

  ${PREFIX}-macos-arm64.zip / ${PREFIX}-macos-x64.zip
  ${PREFIX}-windows-x64.zip     Unzip → Run TTNS Deck.bat if they skip the installer

macOS 12 Monterey (Apple Silicon only): look for *monterey12.dmg or *monterey12.zip

Do NOT Dropbox/email the .app folder — macOS apps are folders and arrive as “Contents”.

macOS Gatekeeper: Done (not Move to Bin) → Privacy & Security → Open Anyway
  (or macos-clear-quarantine.sh)

Windows SmartScreen: More info → Run anyway

Docs: README-TESTERS.txt  USER_GUIDE.md  MACOS_GATEKEEPER.md

Built: $(date -u +"%Y-%m-%dT%H:%MZ")
EOF

echo ""
echo "DJ tester packages in: $OUT"
ls -lh "$OUT"
