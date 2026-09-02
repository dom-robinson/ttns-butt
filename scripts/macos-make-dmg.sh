#!/bin/sh
# Pack TTNS Deck.app and/or TTNS Remote.app into double-clickable UDZO DMGs.
# Usage: macos-make-dmg.sh <dist-macos-dir> <version> <arch-label>
#   arch-label: arm64 | x64 | arm64-monterey12
set -e

DIST="${1:?usage: macos-make-dmg.sh <dist-macos-dir> <version> <arch-label>}"
VER="${2:?}"
LABEL="${3:?}"
DECK_APP="$DIST/TTNS Deck.app"
REMOTE_APP="$DIST/TTNS Remote.app"

make_dmg() {
    app_src="$1"
    app_name="$2"
    out="$3"
    vol="$4"
    readme="$5"

    if [ ! -d "$app_src" ]; then
        echo "ERROR: missing $app_src" >&2
        return 1
    fi

    STAGE="$(mktemp -d "${TMPDIR:-/tmp}/ttns-dmg.XXXXXX")"
    COPYFILE_DISABLE=1 ditto --norsrc --noextattr --noqtn "$app_src" "$STAGE/$app_name"
    ln -s /Applications "$STAGE/Applications"
    printf '%s\n' "$readme" > "$STAGE/READ ME.txt"

    rm -f "$out"
    COPYFILE_DISABLE=1 hdiutil create \
        -volname "$vol" \
        -srcfolder "$STAGE" \
        -ov \
        -format UDZO \
        -fs HFS+ \
        "$out" >/dev/null
    rm -rf "$STAGE"
    echo "DMG: $out"
}

if [ -d "$DECK_APP" ]; then
    make_dmg "$DECK_APP" "TTNS Deck.app" \
        "$DIST/TTNS-Deck-${VER}-macos-${LABEL}.dmg" \
        "TTNS Deck" \
        "TTNS Deck ${VER}

1. Drag “TTNS Deck” into Applications.
2. Open it from Applications.
3. If macOS says it cannot verify the app: click Done (not Move to Bin),
   then System Settings → Privacy & Security → Open Anyway.

Do not email or Dropbox the .app folder itself — send this .dmg file."
fi

if [ -d "$REMOTE_APP" ]; then
    make_dmg "$REMOTE_APP" "TTNS Remote.app" \
        "$DIST/TTNS-Remote-${VER}-macos-${LABEL}.dmg" \
        "TTNS Remote" \
        "TTNS Remote ${VER}

1. Drag “TTNS Remote” into Applications.
2. Open it from Applications.
3. If macOS says it cannot verify the app: click Done (not Move to Bin),
   then System Settings → Privacy & Security → Open Anyway.

Do not email or Dropbox the .app folder itself — send this .dmg file."
fi

if [ ! -d "$DECK_APP" ] && [ ! -d "$REMOTE_APP" ]; then
    echo "ERROR: neither TTNS Deck.app nor TTNS Remote.app in $DIST" >&2
    exit 1
fi
