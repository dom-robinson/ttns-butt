#!/bin/sh
# Pack TTNS Deck.app (+ TTNS Remote.app if present) into a double-clickable UDZO DMG.
# Usage: macos-make-dmg.sh <dist-macos-dir> <version> <arch-label>
#   arch-label: arm64 | x64 | arm64-monterey12
set -e

DIST="${1:?usage: macos-make-dmg.sh <dist-macos-dir> <version> <arch-label>}"
VER="${2:?}"
LABEL="${3:?}"
APP="$DIST/TTNS Deck.app"
REMOTE="$DIST/TTNS Remote.app"
OUT="$DIST/TTNS-Deck-${VER}-macos-${LABEL}.dmg"
VOL="TTNS Deck"

if [ ! -d "$APP" ]; then
    echo "ERROR: missing $APP" >&2
    exit 1
fi

STAGE="$(mktemp -d "${TMPDIR:-/tmp}/ttns-dmg.XXXXXX")"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn "$APP" "$STAGE/TTNS Deck.app"
if [ -d "$REMOTE" ]; then
    COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn "$REMOTE" "$STAGE/TTNS Remote.app"
fi
ln -s /Applications "$STAGE/Applications"

cat > "$STAGE/READ ME.txt" <<EOF
TTNS Deck ${VER}

1. Drag “TTNS Deck” into Applications (and “TTNS Remote” if you are a co-host).
2. Open it from Applications.
3. If macOS says it cannot verify the app: click Done (not Move to Bin),
   then System Settings → Privacy & Security → Open Anyway.

Do not email or Dropbox the .app folder itself — send this .dmg file.
EOF

rm -f "$OUT"
COPYFILE_DISABLE=1 hdiutil create \
    -volname "$VOL" \
    -srcfolder "$STAGE" \
    -ov \
    -format UDZO \
    -fs HFS+ \
    "$OUT" >/dev/null

echo "DMG: $OUT"
