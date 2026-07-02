# TTNS BUTT (TTNS Deck)

TTNS fork of [BUTT](https://github.com/romansavrulin/butt) — live DJ mixer and Icecast client for **The Thursday Night Show**.

| | |
|---|---|
| **Product name** | TTNS Deck |
| **Version** | `0.1.16-ttns-pre.6` (preliminary) |
| **Upstream** | butt 0.1.16 |
| **Repository** | [dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt) |
| **DJ guide** | [`docs/TTNS_DJ_GUIDE.md`](docs/TTNS_DJ_GUIDE.md) |
| **Binary releases** | [`docs/RELEASE.md`](docs/RELEASE.md) |
| **Distribution** | Compiled binaries — macOS (arm64 + Intel), Linux, Windows |

---

## Preliminary release

Tag **`v0.1.16-ttns-pre.6`** — DJ testing build. Download CI artifacts or the GitHub pre-release; see [`docs/RELEASE.md`](docs/RELEASE.md).

```bash
git checkout v0.1.16-ttns-pre.6
./scripts/build-release.sh
```

Licensing: GPL-2.0 fork of butt; see [`docs/LICENSING.md`](docs/LICENSING.md).

---

## Platforms

| OS | Architectures | Package script |
|----|---------------|----------------|
| **macOS** 11+ | Apple Silicon (arm64), Intel (x86_64) | `scripts/build-macos-app.sh` |
| **Linux** | x86_64, arm64 | `scripts/build-linux.sh` |
| **Windows** 10+ | x64 | `scripts/build-windows.sh` (MSYS2) |

CI builds all targets: [`.github/workflows/build.yml`](.github/workflows/build.yml)

---

## Build dependencies

### macOS (Homebrew)

```bash
brew install fltk portaudio lame libvorbis libogg flac opus libsamplerate fdk-aac pkg-config autoconf automake libtool
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh   # → dist/macos/ + TTNS Deck.app with dock icon
```

Apple Silicon uses `/opt/homebrew`; Intel uses `/usr/local` — handled by `configure.ac`.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential autoconf automake libtool pkg-config \
  libfltk1.3-dev libportaudio2 portaudio19-dev libmp3lame-dev \
  libvorbis-dev libogg-dev libflac-dev libopus-dev libsamplerate0-dev libfdk-aac-dev
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh
```

### Windows (MSYS2 MinGW x64)

See `scripts/build-windows.sh` for `pacman` packages, then `bash scripts/build-windows.sh`.

---

## DJ quick start

See **[`docs/TTNS_DJ_GUIDE.md`](docs/TTNS_DJ_GUIDE.md)**.

1. **Settings → Audio** — **Line Input (Deck)** and **Mic Input** (different devices for ducking).
2. Pick **Mount** → **Connect**.
3. **Line**, **Cart**, **Mic** faders; **Gate** / **Depth** for ducking.
4. **LIVE / MUTED** mic button (or **Space**).
5. Carts **1–8**: right-click to assign (WAV/MP3/M4A/FLAC/OGG); click or key to play.
6. **Monitor** under mic button — add voice to headphones (line always monitored).

---

## UI overview

| Area | Controls |
|------|----------|
| Top-left | TTNS logo, About, **mic LIVE/MUTED**, **Monitor** |
| Top | Mount dropdown, Duck indicator |
| Faders | Line, **Cart**, Mic, Gate, Depth |
| Bottom | 8 cart buttons (keys 1–8) |
| Main panel | LCD, transport, VU, More / Settings |

Theme: black background, terminal green, red accents. **TTNS logo** in header, window, dock/taskbar (see [`docs/RELEASE.md`](docs/RELEASE.md)).

---

## Features (preliminary)

- [x] Dual mic/line mix, ducking, 8 carts (WAV/MP3/M4A/FLAC/OGG)
- [x] Master **Cart** fader + per-cart level in setup (live preview)
- [x] TTNS logo + app icon (`.icns` / `.ico`)
- [x] macOS AVFoundation for MP3/M4A carts
- [x] Mic monitor under mic button; line always in headphones
- [x] Cross-platform packages + GitHub Actions CI
- [x] Mount presets (`data/ttns-zones.json`)

---

## Development

- Roadmap: [`docs/TTNS_DEVELOPMENT_PLAN.md`](docs/TTNS_DEVELOPMENT_PLAN.md)
- Changes: [`CHANGELOG.md`](CHANGELOG.md)
- Branch: `ttns-mixer`
- Icons: `./scripts/generate-icons.sh`

```bash
git fetch upstream && git merge upstream/master
```
