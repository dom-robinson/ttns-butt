#!/bin/sh
# Build release package for the current OS.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
case "$(uname -s)" in
    Darwin) exec "$ROOT/scripts/build-macos-app.sh" ;;
    Linux)  exec "$ROOT/scripts/build-linux.sh" ;;
    MING*|MSYS*|CYGWIN*) exec bash "$ROOT/scripts/build-windows.sh" ;;
    *) echo "Unsupported OS for packaging: $(uname -s)" >&2; exit 1 ;;
esac
