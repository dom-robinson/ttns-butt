# TTNS BUTT (TTNS Deck)

TTNS fork of [BUTT](https://github.com/romansavrulin/butt) — live DJ mixer and Icecast client for **The Thursday Night Show**.

| | |
|---|---|
| **Product name** | TTNS Deck |
| **Upstream** | butt 0.1.16 |
| **This fork** | [dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt) (private) |
| **DJ guide** | [`docs/TTNS_DJ_GUIDE.md`](docs/TTNS_DJ_GUIDE.md) |
| **Distribution** | Compiled binaries only — macOS, Linux, Windows |

---

## Platforms

| OS | Architectures | Package script |
|----|---------------|----------------|
| **macOS** 11+ | Apple Silicon (arm64), Intel (x86_64) | `scripts/build-macos-app.sh` |
| **Linux** | x86_64, arm64 | `scripts/build-linux.sh` |
| **Windows** 10+ | x64 | `scripts/build-windows.sh` (MSYS2) |

CI builds all three: [`.github/workflows/build.yml`](.github/workflows/build.yml)

```bash
./scripts/build-release.sh   # auto-picks script for current OS
```

Output: `dist/macos/`, `dist/linux/`, or `dist/windows/`

---

## Build dependencies

### macOS (Homebrew)

```bash
brew install fltk portaudio lame libvorbis libogg flac opus libsamplerate fdk-aac pkg-config autoconf automake libtool
autoreconf -fi && ./configure && make -C src
./src/butt
```

Apple Silicon uses `/opt/homebrew`; Intel uses `/usr/local` — handled by `configure.ac`.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential autoconf automake libtool pkg-config \
  libfltk1.3-dev libportaudio2 libportaudio-dev libmp3lame-dev \
  libvorbis-dev libogg-dev libflac-dev libopus-dev libsamplerate0-dev libfdk-aac-dev
autoreconf -fi && ./configure && make -C src
sudo make -C src install   # installs ttns-deck + /usr/share/ttns-deck/
```

### Windows (MSYS2 MinGW x64)

Install packages listed in `scripts/build-windows.sh`, then `bash scripts/build-windows.sh`.

---

## DJ quick start

See **[`docs/TTNS_DJ_GUIDE.md`](docs/TTNS_DJ_GUIDE.md)** for the full presenter guide.

1. **Settings → Audio** — set **Line Input (Deck)** and **Mic Input** (different devices for ducking).  
2. Pick **Mount** (`ttnszone 1-1` … `4-5`) → **Connect**.  
3. Balance **Line** / **Mic** faders; VU bars in each track follow fader position (post-fader).  
4. Watch **Duck** LED (yellow when ducking).  
5. Use the large **LIVE / MUTED** mic button (or **Space**) to toggle mic on air.  
6. Right-click carts to assign audio; click or press **1–8** to play.  
7. **Settings** (bottom-right) → codec/bitrate if needed. Check **More** after launch for the audio path log line.

Stream target: `decks.thethursdaynightshow.com:8080` (presets in bundled `data/ttns-zones.json`).

---

## UI overview

| Area | Controls |
|------|----------|
| Top-left | TTNS logo, About, **mic LIVE/MUTED** button |
| Top | Mount dropdown, Duck indicator |
| Faders | Line, Mic, Gate, Depth (ducking) |
| Bottom row | **Monitor**, **Mic to Mon** (right-aligned) |
| Carts | 8 buttons with hotkeys |
| Main panel | LCD status, transport, VU meters, More / Settings |

Theme: black background, terminal green text, red accents.

---

## Features

- [x] TTNS logo, window icon, About dialog  
- [x] Dual mic/line mix, ducking, 8 carts, mount presets  
- [x] Large mic mute button + Space toggle  
- [x] Mic monitor + Mic to Mon checkboxes  
- [x] TTNS-themed faders, carts, transport, LCD frame  
- [x] Cross-platform resource paths (app bundle / install dir / dev tree)  
- [x] Post-fader VU meters on Line/Mic faders  
- [x] Reliable dual-device mic capture (macOS)  
- [x] Settings layout cleanup (Audio / Stream / Record / GUI tabs)  

---

## macOS code signing (ops)

Unsigned builds: users right-click → Open. For distribution:

```bash
codesign --force --deep --sign "Developer ID Application: …" "dist/macos/TTNS Deck.app"
xcrun notarytool submit …  # optional notarization
```

---

## Development

- Roadmap: [`docs/TTNS_DEVELOPMENT_PLAN.md`](docs/TTNS_DEVELOPMENT_PLAN.md)  
- Changes: [`CHANGELOG.md`](CHANGELOG.md)  
- Regenerate icons: `scripts/generate-icons.sh`  
- Branch: `ttns-mixer`

```bash
git fetch upstream && git merge upstream/master   # pull upstream butt
```
