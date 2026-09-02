#!/bin/sh
# Assemble DJ tester installers under dist/dj-testers/
# Usage: package-dj-testers.sh [path-to-ci-artifacts-or-dist]
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(tr -d '[:space:]' < "$ROOT/packaging/VERSION")"
OUT="$ROOT/dist/dj-testers"
SEARCH="${1:-$ROOT/dist}"

cd "$ROOT"
mkdir -p "$OUT"

# Keep only the current clearly-named installers.
rm -rf "$OUT"
mkdir -p "$OUT"

copy_first() {
    dest="$1"
    shift
    for pat in "$@"; do
        f="$(find "$SEARCH" -type f -name "$pat" 2>/dev/null | head -1)"
        if [ -n "$f" ]; then
            cp "$f" "$OUT/$dest"
            echo "  $dest"
            return 0
        fi
    done
    echo "  (missing) $dest" >&2
    return 1
}

echo "DJ testers → $OUT"
missing=0
try_copy() {
    copy_first "$@" || missing=$((missing + 1))
}
try_copy "TTNS-Deck-${VER}-macos-arm64.dmg" \
    "TTNS-Deck-${VER}-macos-arm64.dmg"
try_copy "TTNS-Deck-${VER}-macos-x64.dmg" \
    "TTNS-Deck-${VER}-macos-x64.dmg"
try_copy "TTNS-Deck-${VER}-macos-arm64-monterey12.dmg" \
    "TTNS-Deck-${VER}-macos-arm64-monterey12.dmg"
try_copy "TTNS-Deck-${VER}-windows-x64-setup.exe" \
    "TTNS-Deck-${VER}-windows-x64-setup.exe"
try_copy "TTNS-Deck-${VER}-linux-x64.tar.gz" \
    "TTNS-Deck-${VER}-linux-x64.tar.gz" "ttns-deck-linux-x86_64.tar.gz"

try_copy "TTNS-Remote-${VER}-macos-arm64.dmg" \
    "TTNS-Remote-${VER}-macos-arm64.dmg"
try_copy "TTNS-Remote-${VER}-macos-x64.dmg" \
    "TTNS-Remote-${VER}-macos-x64.dmg"
try_copy "TTNS-Remote-${VER}-macos-arm64-monterey12.dmg" \
    "TTNS-Remote-${VER}-macos-arm64-monterey12.dmg"
try_copy "TTNS-Remote-${VER}-windows-x64-setup.exe" \
    "TTNS-Remote-${VER}-windows-x64-setup.exe"
try_copy "TTNS-Remote-${VER}-linux-x64.tar.gz" \
    "TTNS-Remote-${VER}-linux-x64.tar.gz" "ttns-remote-linux-x86_64.tar.gz"

cat > "$OUT/README.txt" <<EOF
TTNS installers ${VER}

Send the FILE, never an extracted .app (Dropbox turns that into a Contents folder).

DECK (DJ / host) — double-click:
  TTNS-Deck-${VER}-macos-arm64.dmg              Apple Silicon, current macOS
  TTNS-Deck-${VER}-macos-x64.dmg                Intel Mac
  TTNS-Deck-${VER}-macos-arm64-monterey12.dmg   Apple Silicon, macOS 12 Monterey
  TTNS-Deck-${VER}-windows-x64-setup.exe        Windows 10+ (no admin)
  TTNS-Deck-${VER}-linux-x64.tar.gz             Linux x86_64 — tar xzf then run-ttns-deck.sh

REMOTE (co-host) — double-click:
  TTNS-Remote-${VER}-macos-arm64.dmg
  TTNS-Remote-${VER}-macos-x64.dmg
  TTNS-Remote-${VER}-macos-arm64-monterey12.dmg
  TTNS-Remote-${VER}-windows-x64-setup.exe
  TTNS-Remote-${VER}-linux-x64.tar.gz            tar xzf then run-ttns-remote.sh

macOS Gatekeeper: Done (not Move to Bin) → Privacy & Security → Open Anyway
Windows SmartScreen: More info → Run anyway

Built: $(date -u +"%Y-%m-%dT%H:%MZ")
EOF

echo ""
ls -lh "$OUT"
if [ "$missing" -ne 0 ]; then
    echo "WARNING: $missing installer(s) not found under $SEARCH" >&2
    exit 1
fi
