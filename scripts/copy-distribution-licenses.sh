#!/bin/sh
# Copy GPL and third-party license files into a release package directory.
# Usage: copy-distribution-licenses.sh <dest-dir>
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:?usage: copy-distribution-licenses.sh <dest-dir>}"

mkdir -p "$DEST/licenses"
cp "$ROOT/COPYING" "$DEST/"
cp "$ROOT/docs/DISTRIBUTION_LICENSE.txt" "$DEST/"
cp "$ROOT/docs/THIRD_PARTY_NOTICES.md" "$DEST/"
cp "$ROOT/docs/licenses/fdk-aac-LICENSE.txt" "$DEST/licenses/"
