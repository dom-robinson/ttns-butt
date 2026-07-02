#!/usr/bin/env bash
# Install MSYS2 MinGW packages for CI, retrying on transient mirror failures.
set -euo pipefail

PACKAGES=(
    mingw-w64-x86_64-toolchain
    mingw-w64-x86_64-make
    mingw-w64-x86_64-fltk
    mingw-w64-x86_64-portaudio
    mingw-w64-x86_64-lame
    mingw-w64-x86_64-libvorbis
    mingw-w64-x86_64-flac
    mingw-w64-x86_64-opus
    mingw-w64-x86_64-libsamplerate
    mingw-w64-x86_64-fdk-aac
    mingw-w64-x86_64-libjpeg-turbo
    mingw-w64-x86_64-libpng
    mingw-w64-x86_64-zlib
    autoconf
    automake
    libtool
    pkg-config
    git
    zip
    mingw-w64-x86_64-python-pillow
)

max_attempts=3
attempt=1

while [ "$attempt" -le "$max_attempts" ]; do
    echo "MSYS2 pacman install attempt ${attempt}/${max_attempts}..."
    if pacman -S --needed --noconfirm "${PACKAGES[@]}"; then
        echo "MSYS2 packages installed."
        exit 0
    fi

    echo "pacman install failed on attempt ${attempt}." >&2
    if [ "$attempt" -lt "$max_attempts" ]; then
        echo "Refreshing package databases and retrying in 20s..." >&2
        pacman -Sy --noconfirm || true
        sleep 20
    fi
    attempt=$((attempt + 1))
done

echo "ERROR: MSYS2 package install failed after ${max_attempts} attempts." >&2
exit 1
