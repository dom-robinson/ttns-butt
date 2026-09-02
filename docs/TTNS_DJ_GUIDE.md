# TTNS Deck — DJ guide

Quick reference for presenters streaming to **decks.thethursdaynightshow.com**.

Works on **macOS** (Apple Silicon and Intel), **Linux**, and **Windows 10+**.

**Build:** `0.1.16-ttns-remote.2` — full guide: [`USER_GUIDE.md`](USER_GUIDE.md). Older mixer-only tag: `v0.1.16-ttns-pre.7`.

---

## Install

Download the build for your platform from [Releases](https://github.com/dom-robinson/ttns-butt/releases). Do **not** share packages publicly — they may contain Icecast source passwords.

| Platform | Package | Run |
|----------|---------|-----|
| **macOS** | `TTNS-Deck-…-macos-arm64.dmg` or `…-macos-x64.dmg` | Open → drag **TTNS Deck** to Applications |
| **macOS 12 Monterey** | `TTNS-Deck-…-macos-arm64-monterey12.dmg` | Apple Silicon only; other Mac DMGs will not launch |
| **Linux** | `ttns-deck-linux-*.tar.gz` | `tar xzf … && ./ttns-deck-linux-*/run-ttns-deck.sh` |
| **Windows** | `TTNS-Deck-…-windows-x64-setup.exe` | Double-click the installer |

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

Open **Settings** (bottom-right) → **Audio** and pick **Line Input (Deck)** and **Mic Input**. They must be **different devices** for ducking and mic monitor to work. If you plug in an interface after Deck is already open, click **Refresh devices**. Meters update ~½ second after you change a device. Click **Save** to persist settings to `~/.buttrc`.

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

Large mic button:

| Look | Meaning |
|------|---------|
| Mic glyph | Mic on air |
| Mic with red **X** | Mic muted |

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

- **Line + carts** are sent to **Monitor Output** only after you pick a real output in Settings → Audio (not “Off”).
- **Monitor** (checkbox under the mic button) — add **mic** to headphones when checked.
- Uncheck **Monitor** if you only want to hear the deck (e.g. Bluetooth mic latency).

Pick **Monitor Output** in Settings → Audio (AirPods vs built-in, etc.).

---

## Settings

**Settings** (above **More**) — codec, bitrate, buffer, recording. TTNS DJs rarely change server fields; **Mount** handles that.

---

## Remotes (co-hosts)

Expand **Remotes** → tick **Accept** → share the room **Code**. Co-hosts run **TTNS Remote** (on the same Mac DMG / Windows installer). Same LAN preferred; internet uses WRX automatically.

**PTT** on the Remotes header: host and guests talk privately — they stay off-air and the music bed is not ducked. Click again when you are ready to put voices back on the stream.

Details: [`USER_GUIDE.md`](USER_GUIDE.md), [`REMOTE_DIALIN.md`](REMOTE_DIALIN.md).

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
| macOS “unidentified developer” / could not verify | **Done**, then Privacy & Security → **Open Anyway** — [`MACOS_GATEKEEPER.md`](MACOS_GATEKEEPER.md) |
| Windows “libFLAC.dll / libfltk not found” | Use the **setup.exe**. If you use the portable zip, unzip fully — do not copy only `ttns-deck.exe` |
| Deck quits when the music player stops (VB-Cable) | Use **v0.1.16-ttns-remote.1** or later |
| Windows crash changing Line/Mic device | Fixed in `v0.1.16-ttns-pre.5` and later |
| macOS crash changing Line/Mic device | Fixed in `v0.1.16-ttns-pre.6` and later |
| Meters stuck / no audio after device change | Fixed in `v0.1.16-ttns-pre.7` — use latest build; wait for “Audio devices ready” in the log |
| Speaker feedback on launch (macOS) | Fixed in `v0.1.16-ttns-pre.7` — monitor stays off until you choose Monitor Output |

---

## Support

- Show ops: your TTNS contact  
- Code: [github.com/dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt)
