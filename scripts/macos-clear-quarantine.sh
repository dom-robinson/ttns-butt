#!/bin/sh
# Clear macOS quarantine flags so unsigned TTNS apps can open after download.
# Usage: macos-clear-quarantine.sh [path-to-app-or-folder ...]
set -e

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 /path/to/TTNS\\ Deck.app [/path/to/TTNS\\ Remote.app ...]"
    echo "   or: $0 /path/to/unzipped-folder"
    exit 1
fi

for path in "$@"; do
    if [ ! -e "$path" ]; then
        echo "Not found: $path" >&2
        exit 1
    fi
    echo "Clearing quarantine: $path"
    xattr -cr "$path"
done

echo "Done. Double-click the app(s) again."
echo "If macOS still blocks: System Settings → Privacy & Security → Open Anyway"
