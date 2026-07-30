#!/bin/sh
# Ad-hoc codesign an .app after dylib bundling / install_name_tool.
# Signs in $TMPDIR to avoid Desktop/iCloud com.apple.provenance blocking codesign.
set -e
APP="${1:?usage: macos-codesign-app.sh /path/to/App.app}"
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
NAME="$(basename "$APP")"

STAGE="$(mktemp -d "${TMPDIR:-/tmp}/ttns-codesign.XXXXXX")"
STAGE_APP="$STAGE/$NAME"

# Clean copy (no resource forks / Finder xattrs)
ditto --norsrc --noextattr "$APP" "$STAGE_APP"
chmod -R u+w "$STAGE_APP"

if [ -d "$STAGE_APP/Contents/Frameworks" ]; then
    find "$STAGE_APP/Contents/Frameworks" -type f \( -name '*.dylib' -o -name '*.so' \) -print0 |
        xargs -0 -n1 codesign --force --sign -
fi

find "$STAGE_APP/Contents/MacOS" -type f -print0 | while IFS= read -r -d '' f; do
    case "$(file -b "$f")" in
        *Mach-O*) codesign --force --sign - "$f" ;;
    esac
done

codesign --force --sign - "$STAGE_APP"
codesign --verify --verbose=1 "$STAGE_APP"

# Replace original with signed tree
rm -rf "$APP"
mkdir -p "$(dirname "$APP")"
ditto --norsrc --noextattr "$STAGE_APP" "$APP"
rm -rf "$STAGE"

echo "Signed: $APP"
