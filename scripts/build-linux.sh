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
if [ -x "$ROOT/src/ttns_remote" ]; then
    cp "$ROOT/src/ttns_remote" "$STAGE/bin/ttns-remote"
    chmod +x "$STAGE/bin/ttns-remote"
fi
cp "$ROOT/data/ttns-zones.json" "$STAGE/share/ttns-deck/data/"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/share/ttns-deck/assets/"
[ -f "$ROOT/assets/ttns-deck.ico" ] && cp "$ROOT/assets/ttns-deck.ico" "$STAGE/share/ttns-deck/assets/" 2>/dev/null || true
mkdir -p "$STAGE/share/icons/hicolor/256x256/apps"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/share/icons/hicolor/256x256/apps/ttns-deck.png"
cp "$ROOT/assets/ttns-deck.desktop" "$STAGE/share/ttns-deck/" 2>/dev/null || true
cp "$ROOT/docs/TTNS_DJ_GUIDE.md" "$STAGE/README.txt"
[ -f "$ROOT/docs/USER_GUIDE.md" ] && cp "$ROOT/docs/USER_GUIDE.md" "$STAGE/share/ttns-deck/" || true
[ -f "$ROOT/docs/REMOTE_DIALIN.md" ] && cp "$ROOT/docs/REMOTE_DIALIN.md" "$STAGE/share/ttns-deck/" || true
[ -f "$ROOT/docs/REMOTE_WAN.md" ] && cp "$ROOT/docs/REMOTE_WAN.md" "$STAGE/share/ttns-deck/" || true

cat > "$STAGE/run-ttns-deck.sh" <<'EOF'
#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$DIR/bin:$PATH"
cd "$DIR/share/ttns-deck" || exit 1
exec "$DIR/bin/ttns-deck" "$@"
EOF
cat > "$STAGE/run-ttns-remote.sh" <<'EOF'
#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$DIR/bin:$PATH"
cd "$DIR/share/ttns-deck" || exit 1
exec "$DIR/bin/ttns-remote" "$@"
EOF
chmod +x "$STAGE/run-ttns-deck.sh" "$STAGE/bin/ttns-deck"
[ -x "$STAGE/bin/ttns-remote" ] && chmod +x "$STAGE/run-ttns-remote.sh"

VER="$(tr -d '[:space:]' < "$ROOT/packaging/VERSION")"
case "$ARCH" in
    x86_64) NAMED_ARCH=x64 ;;
    aarch64|arm64) NAMED_ARCH=arm64 ;;
    *) NAMED_ARCH="$ARCH" ;;
esac

tar -czf "$TARBALL" -C "$DIST" "ttns-deck-linux-${ARCH}"
NAMED_DECK="$DIST/TTNS-Deck-${VER}-linux-${NAMED_ARCH}.tar.gz"
cp "$TARBALL" "$NAMED_DECK"
echo "Built: $TARBALL"
echo "Built: $NAMED_DECK"
echo "Run:   tar xzf $TARBALL && ./ttns-deck-linux-${ARCH}/run-ttns-deck.sh"

if [ -x "$STAGE/bin/ttns-remote" ]; then
    RSTAGE="$DIST/ttns-remote-linux-${ARCH}"
    RTARBALL="$DIST/ttns-remote-linux-${ARCH}.tar.gz"
    rm -rf "$RSTAGE"
    mkdir -p "$RSTAGE/bin" "$RSTAGE/share/ttns-remote/assets" "$RSTAGE/share/ttns-remote/legal"
    "$ROOT/scripts/copy-distribution-licenses.sh" "$RSTAGE/share/ttns-remote/legal"
    cp "$STAGE/bin/ttns-remote" "$RSTAGE/bin/ttns-remote"
    cp "$ROOT/assets/ttns-logo.png" "$RSTAGE/share/ttns-remote/assets/"
    [ -f "$ROOT/docs/USER_GUIDE.md" ] && cp "$ROOT/docs/USER_GUIDE.md" "$RSTAGE/" || true
    cat > "$RSTAGE/run-ttns-remote.sh" <<'EOF'
#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$DIR/bin:$PATH"
cd "$DIR" || exit 1
exec "$DIR/bin/ttns-remote" "$@"
EOF
    chmod +x "$RSTAGE/run-ttns-remote.sh" "$RSTAGE/bin/ttns-remote"
    tar -czf "$RTARBALL" -C "$DIST" "ttns-remote-linux-${ARCH}"
    NAMED_REMOTE="$DIST/TTNS-Remote-${VER}-linux-${NAMED_ARCH}.tar.gz"
    cp "$RTARBALL" "$NAMED_REMOTE"
    echo "Built: $RTARBALL"
    echo "Built: $NAMED_REMOTE"
    echo "Run:   tar xzf $RTARBALL && ./ttns-remote-linux-${ARCH}/run-ttns-remote.sh"
fi
