# TTNS Deck — DJ guide

Quick reference for presenters streaming to **decks.thethursdaynightshow.com**.

Works on **macOS** (Apple Silicon and Intel), **Linux**, and **Windows 10+**.

**Preliminary build:** `v0.1.16-ttns-pre.3` — see [`RELEASE.md`](RELEASE.md) for downloads.

---

## Install

Download the build for your platform (GitHub Actions artifacts or release zip). Do **not** share the zip publicly — it contains Icecast source passwords.

| Platform | Package | Run |
|----------|---------|-----|
| **macOS** | `ttns-deck-arm64-macos.zip` or `ttns-deck-x86_64-macos.zip` | Open **TTNS Deck.app** (right-click → Open if Gatekeeper blocks unsigned builds) |
| **Linux** | `ttns-deck-linux-*.tar.gz` | `tar xzf … && ./ttns-deck-linux-*/run-ttns-deck.sh` |
| **Windows** | `ttns-deck-win64.zip` | Unzip → **Run TTNS Deck.bat** |

First launch creates a config file:

| OS | Config path |
|----|-------------|
| macOS / Linux | `~/.buttrc` |
| Windows | `%USERPROFILE%\.buttrc` |

---

## Before your show

### 1. Audio devices

You need **two inputs**:

| Bus | Use | Examples |
|-----|-----|----------|
| **Deck** (line) | Music / mixer / app audio | USB interface, BlackHole/loopback for Spotify |
| **Mic** | Voice | USB mic, built-in, Bluetooth headset |

Open **Settings** (bottom-right) → **Audio** and pick **Line Input (Deck)** and **Mic Input**. They must be **different devices** for ducking and mic monitor to work.

### 2. Levels

| Control | What it does |
|---------|----------------|
| **Line** | Deck / music level (VU in track = post-fader) |
| **Cart** | Master level for all cart jingles |
| **Mic** | Voice level |
| **Gate** / **Depth** | Ducking when you speak |
| **Duck** (top right) | Yellow when music/carts are ducked |

Per-cart trim: **right-click** a cart → **Level** slider (drag to hear live preview while the cart plays).

### 3. Mount

Choose your **Mount** (e.g. `ttnszone 1-1`). Ops will tell you which slot to use.

### 4. Mic on air

Large button below **About**:

| State | Meaning |
|-------|---------|
| **LIVE** (green) | Mic on air |
| **MUTED** (red) | Mic off air |

Click or press **Space** to toggle.

### 5. Carts (optional)

- **Right-click** cart 1–8 → assign **WAV, MP3, M4A, FLAC, or OGG**
- **Level** in setup — per-jingle trim (live while dragging)
- **Click** or press **1–8** to play (300 ms fade; loop mode in setup)

### 6. Connect

Press **Connect**. Server, mount, password and metadata are filled from the mount preset.

### 7. Record (optional)

**Record** saves post-mix audio to disk. Format in **Settings** → Record.

---

## Headphones / monitor

- **Line + carts** are always sent to **Monitor Output** (Settings → Audio).
- **Monitor** (checkbox under the mic button) — add **mic** to headphones when checked.
- Uncheck **Monitor** if you only want to hear the deck (e.g. Bluetooth mic latency).

Pick **Monitor Output** in Settings → Audio (AirPods vs built-in, etc.).

---

## Settings

**Settings** (above **More**) — codec, bitrate, buffer, recording. TTNS DJs rarely change server fields; **Mount** handles that.

---

## Building from source (developers)

```bash
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh
```

Platform-specific deps: see [`README.TTNS.md`](../README.TTNS.md) and [`RELEASE.md`](RELEASE.md).

---

## Troubleshooting

| Problem | Check |
|---------|--------|
| No connect | Internet, correct Mount, firewall allows outbound :8080 |
| Ducking never triggers | Line and Mic must be separate devices |
| Cart too loud | **Cart** fader and per-cart **Level** in setup |
| MP3 cart crash (old build) | Use `v0.1.16-ttns-pre.1` or newer |
| Carts silent | Supported format; right-click to re-assign |
| macOS “unidentified developer” | Right-click app → Open |
| Windows “libFLAC.dll / libfltk not found” | **Re-download** the latest `ttns-deck-win64.zip` (builds before `v0.1.16-ttns-pre.2` were missing DLLs). Unzip fully, run **Run TTNS Deck.bat** — do not copy only `ttns-deck.exe` |

---

## Support

- Show ops: your TTNS contact  
- Code: [github.com/dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt)
