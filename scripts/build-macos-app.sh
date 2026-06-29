#!/bin/sh
# Build TTNS Deck.app for macOS (Apple Silicon and Intel). Unsigned.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/macos/TTNS Deck.app"

cd "$ROOT"
"$ROOT/scripts/generate-icons.sh" 2>/dev/null || true

if [ ! -x "$ROOT/src/butt" ]; then
    echo "Building binary..."
    autoreconf -fi
    ./configure -q
    make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/data" "$APP/Contents/Resources/assets"

cp "$ROOT/src/butt" "$APP/Contents/MacOS/ttns-deck-bin"
cp "$ROOT/data/ttns-zones.json" "$APP/Contents/Resources/data/"
cp "$ROOT/assets/ttns-logo.png" "$APP/Contents/Resources/assets/"
[ -f "$ROOT/assets/ttns-deck.icns" ] && cp "$ROOT/assets/ttns-deck.icns" "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ttns-deck</string>
    <key>CFBundleIconFile</key>
    <string>ttns-deck</string>
    <key>CFBundleIdentifier</key>
    <string>fm.ttns.deck</string>
    <key>CFBundleName</key>
    <string>TTNS Deck</string>
    <key>CFBundleDisplayName</key>
    <string>TTNS Deck</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.16-ttns</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <string>TTNS Deck needs microphone access for live broadcast mixing.</string>
</dict>
</plist>
EOF

cat > "$APP/Contents/MacOS/ttns-deck" <<'LAUNCHER'
#!/bin/sh
DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR/Resources" || exit 1
exec "$DIR/MacOS/ttns-deck-bin" "$@"
LAUNCHER
chmod +x "$APP/Contents/MacOS/ttns-deck" "$APP/Contents/MacOS/ttns-deck-bin"

ARCH="$(uname -m)"
OUT="$ROOT/dist/macos/ttns-deck-${ARCH}-macos.zip"
rm -f "$OUT"
ditto -c -k --keepParent "$APP" "$OUT"

echo "Built: $APP"
echo "Zip:   $OUT"
echo "Run:   open \"$APP\""
echo ""
echo "Code signing (optional, for distribution outside your Mac):"
echo "  codesign --force --deep --sign \"Developer ID Application: …\" \"$APP\""
