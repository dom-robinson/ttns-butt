#!/bin/sh
# Build TTNS-Deck-*-windows-x64-setup.exe with Inno Setup (optional locally).
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(tr -d '[:space:]' < "$ROOT/packaging/VERSION")"
ISS="$ROOT/packaging/windows/ttns-deck.iss"
STAGE="$ROOT/dist/windows/ttns-deck-win64"

if [ ! -x "$STAGE/bin/ttns-deck.exe" ] && [ ! -f "$STAGE/bin/ttns-deck.exe" ]; then
    echo "ERROR: run scripts/build-windows.sh first (missing $STAGE)" >&2
    exit 1
fi

ISCC=""
for c in \
    "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" \
    "/c/Program Files/Inno Setup 6/ISCC.exe" \
    "$PROGRAMFILES/Inno Setup 6/ISCC.exe" \
    "${PROGRAMFILES(X86)}/Inno Setup 6/ISCC.exe"
do
    if [ -n "$c" ] && [ -f "$c" ]; then
        ISCC="$c"
        break
    fi
done

if [ -z "$ISCC" ]; then
    echo "Inno Setup 6 not found — skipping setup.exe (portable zip is still valid)"
    exit 0
fi

"$ISCC" "/DMyAppVersion=$VER" "$ISS"
echo "Installer: $ROOT/dist/windows/TTNS-Deck-${VER}-windows-x64-setup.exe"
