#!/bin/sh
# Build TTNS Deck for Linux and create a portable tarball.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCH="$(uname -m)"
DIST="$ROOT/dist/linux"
STAGE="$DIST/ttns-deck-linux-${ARCH}"
TARBALL="$DIST/ttns-deck-linux-${ARCH}.tar.gz"

cd "$ROOT"

if [ ! -x "$ROOT/src/butt" ]; then
    echo "Building..."
    autoreconf -fi
    ./configure -q
    make -j"$(nproc 2>/dev/null || echo 4)"
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/share/ttns-deck/data" "$STAGE/share/ttns-deck/assets" "$STAGE/share/ttns-deck/legal"
"$ROOT/scripts/copy-distribution-licenses.sh" "$STAGE/share/ttns-deck/legal"

cp "$ROOT/src/butt" "$STAGE/bin/ttns-deck"
cp "$ROOT/data/ttns-zones.json" "$STAGE/share/ttns-deck/data/"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/share/ttns-deck/assets/"
[ -f "$ROOT/assets/ttns-deck.ico" ] && cp "$ROOT/assets/ttns-deck.ico" "$STAGE/share/ttns-deck/assets/" 2>/dev/null || true
mkdir -p "$STAGE/share/icons/hicolor/256x256/apps"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/share/icons/hicolor/256x256/apps/ttns-deck.png"
cp "$ROOT/assets/ttns-deck.desktop" "$STAGE/share/ttns-deck/" 2>/dev/null || true
cp "$ROOT/docs/TTNS_DJ_GUIDE.md" "$STAGE/README.txt"

cat > "$STAGE/run-ttns-deck.sh" <<'EOF'
#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$DIR/bin:$PATH"
cd "$DIR/share/ttns-deck" || exit 1
exec "$DIR/bin/ttns-deck" "$@"
EOF
chmod +x "$STAGE/run-ttns-deck.sh" "$STAGE/bin/ttns-deck"

tar -czf "$TARBALL" -C "$DIST" "ttns-deck-linux-${ARCH}"
echo "Built: $TARBALL"
echo "Run:   tar xzf $TARBALL && ./ttns-deck-linux-${ARCH}/run-ttns-deck.sh"
