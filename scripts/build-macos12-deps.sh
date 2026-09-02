#!/bin/sh
# Build a macOS 12-compatible third-party prefix (no Homebrew bottles).
# Used by scripts/build-macos12-app.sh so Monterey testers can run TTNS Deck.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIN="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
ARCH="$(uname -m)"
PREFIX="${TTNS_DEP_PREFIX:-$ROOT/deps/macos${MIN%%.*}-$ARCH}"
SRC="$ROOT/deps/src"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
SDKROOT="${SDKROOT:-$(xcrun --show-sdk-path)}"

export MACOSX_DEPLOYMENT_TARGET="$MIN"
export CMAKE_OSX_DEPLOYMENT_TARGET="$MIN"
export SDKROOT
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export CFLAGS="-O2 -mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT"
export CXXFLAGS="-O2 -mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT -stdlib=libc++"
export LDFLAGS="-mmacosx-version-min=$MIN -arch $ARCH -isysroot $SDKROOT"
export PATH="$PREFIX/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export CMAKE_PREFIX_PATH="$PREFIX"

mkdir -p "$PREFIX" "$SRC"

fetch() {
    url="$1"
    dest="$2"
    if [ -f "$dest" ]; then
        return 0
    fi
    echo "Downloading $url"
    curl -L --fail --retry 3 --retry-delay 2 -o "$dest.partial" "$url"
    mv "$dest.partial" "$dest"
}

extract() {
    archive="$1"
    dest="$2"
    if [ -d "$dest" ]; then
        return 0
    fi
    mkdir -p "$SRC/tmp-extract"
    rm -rf "$SRC/tmp-extract/"*
    case "$archive" in
        *.tar.xz) tar -xJf "$archive" -C "$SRC/tmp-extract" ;;
        *.tar.gz|*.tgz) tar -xzf "$archive" -C "$SRC/tmp-extract" ;;
        *.tar.bz2) tar -xjf "$archive" -C "$SRC/tmp-extract" ;;
        *) echo "Unknown archive: $archive" >&2; exit 1 ;;
    esac
    top="$(find "$SRC/tmp-extract" -mindepth 1 -maxdepth 1 -type d | head -1)"
    mv "$top" "$dest"
    rm -rf "$SRC/tmp-extract"
}

have() {
    [ -f "$PREFIX/lib/$1.dylib" ] || [ -f "$PREFIX/lib/$1.a" ]
}

# Apple's modern linker rejects PowerPC-era -force_cpusubtype_ALL.
strip_old_darwin_ldflags() {
    find "$1" \( -name configure -o -name Makefile.in -o -name Makefile.am -o -name CMakeLists.txt \) \
        -print0 2>/dev/null | xargs -0 sed -i '' 's/-force_cpusubtype_ALL//g' 2>/dev/null || true
}

autotools_install() {
    srcdir="$1"
    shift
    strip_old_darwin_ldflags "$srcdir"
    if [ ! -x "$srcdir/configure" ]; then
        (cd "$srcdir" && if [ -x ./autogen.sh ]; then ./autogen.sh; else autoreconf -fi; fi)
        strip_old_darwin_ldflags "$srcdir"
    fi
    (
        cd "$srcdir"
        if [ -f Makefile ]; then
            make distclean >/dev/null 2>&1 || true
        fi
        ./configure --prefix="$PREFIX" --disable-dependency-tracking --disable-silent-rules "$@"
        make -j"$JOBS"
        make install
    )
}

cmake_install() {
    srcdir="$1"
    shift
    build="$srcdir/build-ttns"
    cmake -S "$srcdir" -B "$build" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DCMAKE_OSX_SYSROOT="$SDKROOT" \
        -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DCMAKE_FIND_FRAMEWORK=LAST \
        -DCMAKE_C_FLAGS="-mmacosx-version-min=$MIN" \
        -DCMAKE_CXX_FLAGS="-mmacosx-version-min=$MIN -stdlib=libc++" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_SHARED_LIBS=ON \
        "$@"
    cmake --build "$build" -j"$JOBS"
    cmake --install "$build"
}

echo "macOS $MIN prefix: $PREFIX"

# --- image / codec deps ---
if ! have libjpeg; then
    fetch "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.3/libjpeg-turbo-3.0.3.tar.gz" \
        "$SRC/libjpeg-turbo-3.0.3.tar.gz"
    extract "$SRC/libjpeg-turbo-3.0.3.tar.gz" "$SRC/libjpeg-turbo-3.0.3"
    cmake_install "$SRC/libjpeg-turbo-3.0.3" -DENABLE_SHARED=ON -DENABLE_STATIC=ON
fi

if ! have libpng16; then
    fetch "https://download.sourceforge.net/libpng/libpng-1.6.43.tar.xz" \
        "$SRC/libpng-1.6.43.tar.xz"
    extract "$SRC/libpng-1.6.43.tar.xz" "$SRC/libpng-1.6.43"
    autotools_install "$SRC/libpng-1.6.43"
fi

if ! have libogg; then
    fetch "https://github.com/xiph/ogg/releases/download/v1.3.5/libogg-1.3.5.tar.xz" \
        "$SRC/libogg-1.3.5.tar.xz"
    extract "$SRC/libogg-1.3.5.tar.xz" "$SRC/libogg-1.3.5"
    autotools_install "$SRC/libogg-1.3.5"
fi

if ! have libopus; then
    fetch "https://github.com/xiph/opus/releases/download/v1.5.2/opus-1.5.2.tar.gz" \
        "$SRC/opus-1.5.2.tar.gz"
    extract "$SRC/opus-1.5.2.tar.gz" "$SRC/opus-1.5.2"
    autotools_install "$SRC/opus-1.5.2"
fi

if ! have libvorbis; then
    fetch "https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.xz" \
        "$SRC/libvorbis-1.3.7.tar.xz"
    extract "$SRC/libvorbis-1.3.7.tar.xz" "$SRC/libvorbis-1.3.7"
    autotools_install "$SRC/libvorbis-1.3.7"
fi

if ! have libFLAC; then
    fetch "https://github.com/xiph/flac/releases/download/1.4.3/flac-1.4.3.tar.xz" \
        "$SRC/flac-1.4.3.tar.xz"
    extract "$SRC/flac-1.4.3.tar.xz" "$SRC/flac-1.4.3"
    cmake_install "$SRC/flac-1.4.3" -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF
fi

if ! have libmp3lame; then
    fetch "https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz" \
        "$SRC/lame-3.100.tar.gz"
    extract "$SRC/lame-3.100.tar.gz" "$SRC/lame-3.100"
    # lame 3.100 exports lame_init_old which is not built with modern clang.
    sed -i '' '/lame_init_old/d' "$SRC/lame-3.100/include/libmp3lame.sym"
    CFLAGS="$CFLAGS -Wno-error=implicit-function-declaration" \
        autotools_install "$SRC/lame-3.100" --disable-frontend --disable-gtktest --disable-nasm
fi

if ! have libsamplerate; then
    fetch "https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz" \
        "$SRC/libsamplerate-0.2.2.tar.xz"
    extract "$SRC/libsamplerate-0.2.2.tar.xz" "$SRC/libsamplerate-0.2.2"
    cmake_install "$SRC/libsamplerate-0.2.2" -DBUILD_TESTING=OFF -DLIBSAMPLERATE_EXAMPLES=OFF
fi

if ! have libfdk-aac; then
    fetch "https://github.com/mstorsjo/fdk-aac/archive/refs/tags/v2.0.3.tar.gz" \
        "$SRC/fdk-aac-2.0.3.tar.gz"
    extract "$SRC/fdk-aac-2.0.3.tar.gz" "$SRC/fdk-aac-2.0.3"
    autotools_install "$SRC/fdk-aac-2.0.3"
fi

if ! have libportaudio; then
    fetch "https://github.com/PortAudio/portaudio/archive/refs/tags/v19.7.0.tar.gz" \
        "$SRC/portaudio-19.7.0.tar.gz"
    extract "$SRC/portaudio-19.7.0.tar.gz" "$SRC/portaudio-19.7.0"
    cmake_install "$SRC/portaudio-19.7.0" -DPA_BUILD_SHARED=ON -DPA_BUILD_STATIC=ON -DPA_BUILD_TESTS=OFF -DPA_BUILD_EXAMPLES=OFF
fi

if ! have libcurl; then
    fetch "https://curl.se/download/curl-8.11.1.tar.xz" \
        "$SRC/curl-8.11.1.tar.xz"
    extract "$SRC/curl-8.11.1.tar.xz" "$SRC/curl-8.11.1"
    ws_flag="--enable-websockets"
    if ! (cd "$SRC/curl-8.11.1" && ./configure --help | grep -q enable-websockets); then
        ws_flag=""
    fi
    autotools_install "$SRC/curl-8.11.1" \
        --with-secure-transport \
        --without-libpsl \
        --without-brotli \
        --without-zstd \
        --without-nghttp2 \
        --without-libidn2 \
        --disable-ldap \
        --disable-ldaps \
        $ws_flag
fi

if ! have libfltk; then
    fetch "https://github.com/fltk/fltk/releases/download/release-1.4.5/fltk-1.4.5-source.tar.gz" \
        "$SRC/fltk-1.4.5-source.tar.gz"
    extract "$SRC/fltk-1.4.5-source.tar.gz" "$SRC/fltk-1.4.5"
    cmake_install "$SRC/fltk-1.4.5" \
        -DFLTK_BUILD_TEST=OFF \
        -DFLTK_BUILD_FLUID=OFF \
        -DFLTK_BUILD_EXAMPLES=OFF \
        -DFLTK_USE_SYSTEM_LIBJPEG=ON \
        -DFLTK_USE_SYSTEM_LIBPNG=ON \
        -DFLTK_USE_SYSTEM_ZLIB=ON
fi

if [ ! -x "$PREFIX/bin/fltk-config" ]; then
    echo "ERROR: fltk-config missing from $PREFIX/bin" >&2
    exit 1
fi

echo "$MIN" > "$PREFIX/.deployment-target"
echo "Installed to $PREFIX"
"$PREFIX/bin/fltk-config" --version
pkg-config --modversion libcurl || true
