#!/bin/sh
# Generate macOS .icns and Windows .ico from assets/ttns-logo.png
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/assets/ttns-logo.png"
ICNS="$ROOT/assets/ttns-deck.icns"
ICO="$ROOT/assets/ttns-deck.ico"
ICONSET="$ROOT/build/icon.iconset"

if [ ! -f "$SRC" ]; then
    echo "Missing $SRC" >&2
    exit 1
fi

mkdir -p "$ROOT/build" "$ICONSET"

sips -z 16 16     "$SRC" --out "$ICONSET/icon_16x16.png"      >/dev/null
sips -z 32 32     "$SRC" --out "$ICONSET/icon_16x16@2x.png"   >/dev/null
sips -z 32 32     "$SRC" --out "$ICONSET/icon_32x32.png"      >/dev/null
sips -z 64 64     "$SRC" --out "$ICONSET/icon_32x32@2x.png"   >/dev/null
sips -z 128 128   "$SRC" --out "$ICONSET/icon_128x128.png"    >/dev/null
sips -z 256 256   "$SRC" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -z 256 256   "$SRC" --out "$ICONSET/icon_256x256.png"    >/dev/null
sips -z 512 512   "$SRC" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -z 512 512   "$SRC" --out "$ICONSET/icon_512x512.png"    >/dev/null
cp "$SRC" "$ICONSET/icon_512x512@2x.png"

iconutil -c icns "$ICONSET" -o "$ICNS"
echo "Wrote $ICNS"

if command -v magick >/dev/null 2>&1; then
    magick "$SRC" -define icon:auto-resize=256,128,64,48,32,16 "$ICO"
    echo "Wrote $ICO (ImageMagick)"
elif command -v convert >/dev/null 2>&1; then
    convert "$SRC" -define icon:auto-resize=256,128,64,48,32,16 "$ICO"
    echo "Wrote $ICO (ImageMagick convert)"
elif python3 -c "import PIL" 2>/dev/null; then
    python3 - "$SRC" "$ICO" <<'PY'
import sys
from PIL import Image
img = Image.open(sys.argv[1]).convert("RGBA")
img.save(sys.argv[2], format="ICO", sizes=[(256,256),(128,128),(64,64),(48,48),(32,32),(16,16)])
PY
    echo "Wrote $ICO (Pillow)"
else
    echo "Skip .ico — install ImageMagick or python3-pillow for Windows icon" >&2
fi
