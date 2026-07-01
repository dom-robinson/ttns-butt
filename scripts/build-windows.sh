# Build TTNS Deck on Windows 10+ (MSYS2 / MinGW64).
# Run inside "MSYS2 MinGW x64" terminal:
#   pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-fltk \
#     mingw-w64-x86_64-portaudio mingw-w64-x86_64-lame mingw-w64-x86_64-libvorbis \
#     mingw-w64-x86_64-flac mingw-w64-x86_64-opus mingw-w64-x86_64-libsamplerate \
#     mingw-w64-x86_64-fdk-aac autoconf automake libtool pkg-config git
#
# Then from repo root:
#   bash scripts/build-windows.sh

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist/windows"
STAGE="$DIST/ttns-deck-win64"
ZIP="$DIST/ttns-deck-win64.zip"
EXE="$STAGE/bin/ttns-deck.exe"
MINGW_BIN="${MINGW_PREFIX:-/mingw64}/bin"
BUNDLED_LIST="$STAGE/.bundled_dlls"

cd "$ROOT"
[ -f "$ROOT/scripts/generate-icons.sh" ] && bash "$ROOT/scripts/generate-icons.sh" 2>/dev/null || true

if [ ! -f "$ROOT/src/butt.exe" ] && [ ! -f "$ROOT/src/butt" ]; then
    autoreconf -fi
    ./configure -q --disable-dependency-tracking
    mingw32-make -C src -j"$(nproc 2>/dev/null || echo 4)"
else
    echo "Using existing binary in src/"
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/data" "$STAGE/assets"
: > "$BUNDLED_LIST"

cp "$ROOT/src/butt.exe" "$STAGE/bin/ttns-deck.exe" 2>/dev/null || cp "$ROOT/src/butt" "$STAGE/bin/ttns-deck.exe"
cp "$ROOT/data/ttns-zones.json" "$STAGE/data/"
cp "$ROOT/assets/ttns-logo.png" "$STAGE/assets/"
cp "$ROOT/docs/TTNS_DJ_GUIDE.md" "$STAGE/README.txt"

is_system_dll() {
    case "$(echo "$1" | tr '[:upper:]' '[:lower:]')" in
        advapi32.dll|bcrypt.dll|cfgmgr32.dll|combase.dll|comctl32.dll|comdlg32.dll|\
        crypt32.dll|dnsapi.dll|dwmapi.dll|gdi32.dll|imm32.dll|iphlpapi.dll|\
        kernel32.dll|kernelbase.dll|msimg32.dll|msvcrt.dll|ntdll.dll|ole32.dll|\
        oleaut32.dll|powrprof.dll|profapi.dll|rpcrt4.dll|secur32.dll|setupapi.dll|\
        shell32.dll|shlwapi.dll|user32.dll|userenv.dll|uuid.dll|version.dll|\
        winmm.dll|ws2_32.dll|wtsapi32.dll|uxtheme.dll|windows.storage.dll) return 0 ;;
    esac
    return 1
}

already_bundled() {
    grep -Fqx "$1" "$BUNDLED_LIST" 2>/dev/null
}

find_mingw_dll() {
    local dll="$1"
    local dir
    for dir in "$MINGW_BIN" /mingw64/bin; do
        if [ -f "$dir/$dll" ]; then
            echo "$dir/$dll"
            return 0
        fi
    done
    return 1
}

copy_mingw_dll() {
    local dll="$1"
    local src

    already_bundled "$dll" && return 0
    is_system_dll "$dll" && return 0

    src="$(find_mingw_dll "$dll" || true)"
    if [ -z "$src" ]; then
        echo "ERROR: required DLL not found in MSYS2: $dll" >&2
        return 1
    fi

    cp "$src" "$STAGE/bin/"
    echo "$dll" >> "$BUNDLED_LIST"
    echo "Bundled $dll"
    return 0
}

pe_import_dlls() {
    local pe="$1"
    objdump -p "$pe" 2>/dev/null | awk '/DLL Name:/ {print $3}'
}

bundle_pe_tree() {
    local queue=("$1")
    local pe dll

    while [ "${#queue[@]}" -gt 0 ]; do
        pe="${queue[0]}"
        queue=("${queue[@]:1}")

        while IFS= read -r dll; do
            [ -n "$dll" ] || continue
            is_system_dll "$dll" && continue
            if ! already_bundled "$dll"; then
                copy_mingw_dll "$dll"
                queue+=("$STAGE/bin/$dll")
            fi
        done < <(pe_import_dlls "$pe")
    done
}

# Bundle MinGW runtime + codec/GUI DLLs next to exe (required on PCs without MSYS2).
# Seed list covers common names; bundle_pe_tree then walks imports recursively so
# FLTK 1.4 / libFLAC.dll renames and transitive deps (jpeg/png/zlib) are picked up.
for dll in \
    libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
    libfltk-1.4.dll libfltk_images-1.4.dll libportaudio.dll \
    libmp3lame-0.dll libvorbis-0.dll libvorbisenc-2.dll libvorbisfile-3.dll libogg-0.dll \
    libopus-0.dll libFLAC.dll libsamplerate-0.dll libfdk-aac-2.dll \
    zlib1.dll libpng16-16.dll libjpeg-8.dll
do
    copy_mingw_dll "$dll" || true
done

if ! command -v objdump >/dev/null 2>&1; then
    echo "ERROR: objdump is required to verify Windows DLL bundling" >&2
    exit 1
fi

bundle_pe_tree "$EXE"

missing=()
while IFS= read -r dll; do
    [ -n "$dll" ] || continue
    is_system_dll "$dll" && continue
    if [ ! -f "$STAGE/bin/$dll" ]; then
        missing+=("$dll")
    fi
done < <(pe_import_dlls "$EXE")

if [ "${#missing[@]}" -gt 0 ]; then
    echo "ERROR: ttns-deck.exe still missing bundled DLLs: ${missing[*]}" >&2
    exit 1
fi

rm -f "$BUNDLED_LIST"

cat > "$STAGE/Run TTNS Deck.bat" <<'EOF'
@echo off
cd /d "%~dp0"
bin\ttns-deck.exe %*
EOF

mkdir -p "$DIST"
rm -f "$ZIP"
(cd "$DIST" && powershell -Command "Compress-Archive -Path 'ttns-deck-win64' -DestinationPath 'ttns-deck-win64.zip' -Force" 2>/dev/null) \
    || (cd "$DIST" && zip -r ttns-deck-win64.zip ttns-deck-win64)

echo "Built: $STAGE"
echo "Zip:   $ZIP"
