# TTNS Deck — DJ guide

Quick reference for presenters streaming to **decks.thethursdaynightshow.com**.

Works on **macOS** (Apple Silicon and Intel), **Linux**, and **Windows 10+**.

---

## Install

Download the build for your platform from your TTNS contact (GitHub Actions artifacts or release zip). Do **not** share the zip publicly — it contains Icecast source passwords.

| Platform | Package | Run |
|----------|---------|-----|
| **macOS** | `TTNS Deck.app` or `ttns-deck-*-macos.zip` | Open the app (right-click → Open if Gatekeeper blocks unsigned builds) |
| **Linux** | `ttns-deck-linux-*.tar.gz` | `tar xzf … && ./ttns-deck-linux-*/run-ttns-deck.sh` |
| **Windows** | `ttns-deck-win64.zip` | Unzip → double-click **Run TTNS Deck.bat** |

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

Open **Settings** (bottom-right) → **Audio** tab and pick **Line Input (Deck)** and **Mic Input**. They must be **different devices** for ducking and mic monitor to work.

### 2. Levels

- **Line** fader — music / deck level (VU meter in the track reflects fader gain)  
- **Mic** fader — voice level (meter follows fader and mute state)  
- **Gate** / **Depth** — ducking threshold and how far music drops when you speak  
- **Duck** indicator (top right) turns **yellow** when music is being ducked  

After launch, open **More** and look for a line like `TTNS audio: deck=… mic=…` — that confirms separate devices are in use.

### 3. Mount

Choose your **Mount** from the dropdown (e.g. `ttnszone 1-1`). These map to Icecast mounts — your scheduler or ops team will tell you which to use.

### 4. Mic on air

The large button below **About** (left side) shows your mic status:

| State | Look | Meaning |
|-------|------|---------|
| **LIVE** | Green mic icon | Mic is on air |
| **MUTED** | Red mic with strike-through | Mic is off air |

**Click** the button or press **Space** to toggle. Same behaviour either way.

### 5. Carts (optional)

- **Right-click** a cart button (1–8) → pick audio (**WAV, MP3, FLAC, or OGG**)
- **Click** or press **1–8** to play (300 ms fade in/out)  
- Labels show the first four characters of the filename  

### 6. Connect

Press **Connect** (play button on the main panel). Server, mount, password and stream metadata are filled in automatically.

### 7. Record (optional)

Press **Record** to save a post-mix WAV/MP3 to disk (same audio as the stream). Set format in **Settings** → Audio/Record tabs.

---

## Mic monitor

On the row below the faders (right-aligned):

- **Monitor** — hear your mic in headphones (default system output)  
- **Mic to Mon** — silence the monitor without affecting the broadcast  

Useful to avoid Bluetooth latency when you don’t want to hear yourself.

---

## Settings

Click **Settings** (above **More**, bottom-right) for codec (MP3/OGG/Opus/AAC), bitrate, buffer size, and recording options. TTNS DJs rarely need to change server settings — Mount handles that.

The **GUI** tab only has window behaviour options (attach, always on top, LCD auto-cycle). Display colours are fixed by the TTNS theme.

---

## Building from source (developers)

### macOS

```bash
brew install fltk portaudio lame libvorbis libogg flac opus libsamplerate fdk-aac pkg-config autoconf automake libtool
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh   # → dist/macos/
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential autoconf automake libtool pkg-config \
  libfltk1.3-dev libportaudio2 libportaudio-dev libmp3lame-dev \
  libvorbis-dev libogg-dev libflac-dev libopus-dev libsamplerate0-dev libfdk-aac-dev
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh   # → dist/linux/
```

### Windows (MSYS2 MinGW64)

See comments in `scripts/build-windows.sh` for `pacman` packages, then:

```bash
bash scripts/build-windows.sh
```

---

## Troubleshooting

| Problem | Check |
|---------|--------|
| No connect | Internet, correct Mount, firewall allows outbound :8080 |
| Ducking never triggers | Line and Mic must be separate devices |
| Mic level on wrong fader | Same device selected for Line and Mic — use deck loopback + separate mic |
| Mic meter flat but line moves | Mic device failed to open; check **More** log and **Settings → Audio** |
| Carts silent | Assign a supported file (WAV/MP3/FLAC/OGG); right-click to re-assign |
| Mic won’t unmute | Click LIVE/MUTED button or Space — both toggle the same state |
| macOS “unidentified developer” | Right-click app → Open, or ops team re-signs build |
| Windows missing DLL | Use the full zip from `build-windows.sh`, not bare exe |

---

## Support

- Show ops / scheduler: your TTNS contact  
- Technical fork issues: [github.com/dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt) (private)
