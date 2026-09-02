#!/bin/sh
# Collect platform release archives from CI artifact trees or local dist/.
# Usage: stage-release-packages.sh <output-dir> [search-root ...]
set -e

OUT="${1:?usage: stage-release-packages.sh <output-dir> [search-root ...]}"
shift

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [ "$#" -eq 0 ]; then
    set -- "$ROOT/dist"
fi

rm -rf "$OUT"
mkdir -p "$OUT"

copy_named() {
    name="$1"
    for search in $SEARCH_ROOTS; do
        [ -d "$search" ] || continue
        f="$(find "$search" -type f -name "$name" 2>/dev/null | head -1)"
        if [ -n "$f" ]; then
            cp "$f" "$OUT/$name"
            echo "Staged $name"
            return 0
        fi
    done
    return 1
}

SEARCH_ROOTS="$*"

WANT="
ttns-deck-arm64-macos.zip
ttns-deck-x86_64-macos.zip
ttns-deck-linux-x86_64.tar.gz
ttns-deck-win64.zip
"

for name in $WANT; do
    copy_named "$name" || true
done

for search in $SEARCH_ROOTS; do
    [ -d "$search" ] || continue
    find "$search" -type f \( \
        -name 'TTNS-Deck-*.dmg' -o \
        -name 'TTNS-Remote-*.dmg' -o \
        -name 'TTNS-Deck-*-windows-x64-setup.exe' -o \
        -name 'TTNS-Remote-*-windows-x64-setup.exe' -o \
        -name 'TTNS-Deck-*-linux-*.tar.gz' -o \
        -name 'TTNS-Remote-*-linux-*.tar.gz' -o \
        -name 'ttns-remote-linux-*.tar.gz' \
        \) 2>/dev/null | while read -r f; do
        base="$(basename "$f")"
        if [ ! -f "$OUT/$base" ]; then
            cp "$f" "$OUT/$base"
            echo "Staged $base"
        fi
    done
done

missing=0
for name in $WANT; do
    if [ ! -f "$OUT/$name" ]; then
        echo "Missing: $name" >&2
        missing=1
    fi
done

if [ "$missing" -ne 0 ]; then
    echo "ERROR: not all portable release packages found under: $*" >&2
    exit 1
fi

ls -lh "$OUT"
