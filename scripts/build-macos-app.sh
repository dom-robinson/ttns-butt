#!/bin/sh
# Build TTNS Deck.app and TTNS Remote.app for macOS (Apple Silicon and Intel). Unsigned.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/macos/TTNS Deck.app"
REMOTE_APP="$ROOT/dist/macos/TTNS Remote.app"
VER="0.1.16-ttns-remote-dev.4"

cd "$ROOT"
"$ROOT/scripts/generate-icons.sh" 2>/dev/null || true

echo "Building binary..."
if [ ! -f "$ROOT/configure" ]; then
    autoreconf -fi
    ./configure -q
fi
make -C "$ROOT/src" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

rm -rf "$APP" "$REMOTE_APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/data" "$APP/Contents/Resources/assets"
"$ROOT/scripts/copy-distribution-licenses.sh" "$APP/Contents/Resources/legal"

cp "$ROOT/src/butt" "$APP/Contents/MacOS/ttns-deck-bin"
cp "$ROOT/data/ttns-zones.json" "$APP/Contents/Resources/data/"
cp "$ROOT/assets/ttns-logo.png" "$APP/Contents/Resources/assets/"
[ -f "$ROOT/assets/ttns-deck.icns" ] && cp "$ROOT/assets/ttns-deck.icns" "$APP/Contents/Resources/"

cp "$ROOT/docs/TTNS_DJ_GUIDE.md" "$APP/Contents/Resources/README.txt" 2>/dev/null || true
[ -f "$ROOT/docs/USER_GUIDE.md" ] && cp "$ROOT/docs/USER_GUIDE.md" "$APP/Contents/Resources/" || true
[ -f "$ROOT/docs/REMOTE_DIALIN.md" ] && cp "$ROOT/docs/REMOTE_DIALIN.md" "$APP/Contents/Resources/" || true
[ -f "$ROOT/docs/REMOTE_WAN.md" ] && cp "$ROOT/docs/REMOTE_WAN.md" "$APP/Contents/Resources/" || true
[ -f "$ROOT/docs/MACOS_GATEKEEPER.md" ] && cp "$ROOT/docs/MACOS_GATEKEEPER.md" "$APP/Contents/Resources/" || true

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ttns-deck-bin</string>
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
    <string>${VER}</string>
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

chmod +x "$APP/Contents/MacOS/ttns-deck-bin"

# Bundle Homebrew dylibs so the app runs on machines without brew.
python3 "$ROOT/scripts/macos-bundle-dylibs.py" "$APP" "Contents/MacOS/ttns-deck-bin"
chmod +x "$ROOT/scripts/macos-codesign-app.sh"
"$ROOT/scripts/macos-codesign-app.sh" "$APP"

ARCH="$(uname -m)"
OUT="$ROOT/dist/macos/ttns-deck-${ARCH}-macos.zip"
rm -f "$OUT"
COPYFILE_DISABLE=1 ditto -c -k --keepParent "$APP" "$OUT"

echo "Built: $APP"
echo "Zip:   $OUT"
echo "Run:   open \"$APP\""

if [ -x "$ROOT/src/ttns_remote" ]; then
    mkdir -p "$REMOTE_APP/Contents/MacOS" "$REMOTE_APP/Contents/Resources/assets"
    "$ROOT/scripts/copy-distribution-licenses.sh" "$REMOTE_APP/Contents/Resources/legal"
    cp "$ROOT/src/ttns_remote" "$REMOTE_APP/Contents/MacOS/ttns-remote-bin"
    cp "$ROOT/assets/ttns-logo.png" "$REMOTE_APP/Contents/Resources/assets/"
    [ -f "$ROOT/assets/ttns-deck.icns" ] && cp "$ROOT/assets/ttns-deck.icns" "$REMOTE_APP/Contents/Resources/ttns-deck.icns"
    [ -f "$ROOT/docs/USER_GUIDE.md" ] && cp "$ROOT/docs/USER_GUIDE.md" "$REMOTE_APP/Contents/Resources/" || true
    [ -f "$ROOT/docs/MACOS_GATEKEEPER.md" ] && cp "$ROOT/docs/MACOS_GATEKEEPER.md" "$REMOTE_APP/Contents/Resources/" || true

    cat > "$REMOTE_APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ttns-remote-bin</string>
    <key>CFBundleIconFile</key>
    <string>ttns-deck</string>
    <key>CFBundleIdentifier</key>
    <string>fm.ttns.remote</string>
    <key>CFBundleName</key>
    <string>TTNS Remote</string>
    <key>CFBundleDisplayName</key>
    <string>TTNS Remote</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VER}</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <string>TTNS Remote needs microphone access for co-host dial-in.</string>
</dict>
</plist>
EOF

    chmod +x "$REMOTE_APP/Contents/MacOS/ttns-remote-bin"

    # Keep a CLI copy for developers; crew should use the .app
    cp "$ROOT/src/ttns_remote" "$ROOT/dist/macos/ttns-remote"
    mkdir -p "$ROOT/dist/macos/assets"
    cp "$ROOT/assets/ttns-logo.png" "$ROOT/dist/macos/assets/"

    python3 "$ROOT/scripts/macos-bundle-dylibs.py" "$REMOTE_APP" "Contents/MacOS/ttns-remote-bin"
    "$ROOT/scripts/macos-codesign-app.sh" "$REMOTE_APP"

    REMOTE_ZIP="$ROOT/dist/macos/ttns-remote-${ARCH}-macos.zip"
    rm -f "$REMOTE_ZIP"
    COPYFILE_DISABLE=1 ditto -c -k --keepParent "$REMOTE_APP" "$REMOTE_ZIP"
    echo "Remote app: \"$REMOTE_APP\""
    echo "Remote zip: $REMOTE_ZIP"
fi

echo ""
echo "Unsigned builds from the internet need a Gatekeeper bypass — see docs/MACOS_GATEKEEPER.md"
echo "Optional Developer ID signing:"
echo "  codesign --force --deep --sign \"Developer ID Application: …\" \"$APP\""
