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

WANT="
ttns-deck-arm64-macos.zip
ttns-deck-x86_64-macos.zip
ttns-deck-linux-x86_64.tar.gz
ttns-deck-win64.zip
"

for search in "$@"; do
    [ -d "$search" ] || continue
    for name in $WANT; do
        f="$(find "$search" -type f -name "$name" 2>/dev/null | head -1)"
        if [ -n "$f" ]; then
            cp "$f" "$OUT/$name"
            echo "Staged $name"
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
    echo "ERROR: not all release packages found under: $*" >&2
    exit 1
fi

ls -lh "$OUT"
