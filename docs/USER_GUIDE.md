# TTNS Deck & TTNS Remote — User Guide

**Build:** `0.1.16-ttns-remote-dev.2` (crew test build on branch `feature/remote-dial-in`)

This guide covers day-to-day use of **TTNS Deck** (the mixer / streamer) and **TTNS Remote** (the co-host dial-in app). Drop screenshots into `docs/images/user-guide/` using the filenames noted under each section.

> **Ops note:** Do not redistribute installer zips publicly — packages may include Icecast mount credentials for show use.

---

## Contents

1. [What you need](#what-you-need)
2. [Install](#install)
3. [TTNS Deck — first run](#ttns-deck--first-run)
4. [Streaming a show](#streaming-a-show)
5. [Carts and ducking](#carts-and-ducking)
6. [Headphones / monitor](#headphones--monitor)
7. [Remotes (co-hosts)](#remotes-co-hosts)
8. [TTNS Remote app](#ttns-remote-app)
9. [Telephone / core reach light](#telephone--core-reach-light)
10. [Settings cheat sheet](#settings-cheat-sheet)
11. [Logs and feedback](#logs-and-feedback)
12. [Troubleshooting](#troubleshooting)

---

## What you need

| Role | App | Hardware |
|------|-----|----------|
| **DJ / host** | TTNS Deck | Computer, line/music source, mic, headphones |
| **Co-host** | TTNS Remote | Computer or second machine, mic, headphones |

Same Wi‑Fi/LAN is ideal for remotes. Over the internet, Deck and Remote use the WRX relay automatically when LAN discovery fails.

---

## Install

| Platform | Package | How to run |
|----------|---------|------------|
| **macOS Apple Silicon** | `TTNS-Deck-…-macos-arm64.zip` | Unzip → open **TTNS Deck.app** (right-click → **Open** if Gatekeeper blocks) |
| **macOS Intel** | `TTNS-Deck-…-macos-x64.zip` | Same |
| **Linux x86_64** | `TTNS-Deck-…-linux-x64.tar.gz` | `tar xzf … && ./ttns-deck-linux-*/run-ttns-deck.sh` |
| **Windows 10+** | `TTNS-Deck-…-windows-x64.zip` | Unzip fully → **Run TTNS Deck.bat** |

**TTNS Remote** is included in the same packages:

| Platform | Remote binary |
|----------|---------------|
| macOS | Inside the app bundle, and/or `ttns-remote` / `TTNS-Remote-…-macos-*.zip` in the handoff folder |
| Linux | `./ttns-deck-linux-*/run-ttns-remote.sh` |
| Windows | `bin\ttns-remote.exe` (same unzip folder as Deck) |

<!-- Screenshot: Finder/Explorer showing unzipped package -->
![Install package contents](images/user-guide/01-install-package.png)

---

## TTNS Deck — first run

On launch, Deck creates a config file:

| OS | Config |
|----|--------|
| macOS / Linux | `~/.buttrc` |
| Windows | `%USERPROFILE%\.buttrc` |

### Main window layout

Top to bottom (typical):

1. **LCD** — status / stream info
2. **Faders** — Line, Cart, Mic (+ Gate / Depth for ducking)
3. **Carts** — eight jingle pads
4. **Transport** — Record / Stop / Play, mic mute, Monitor
5. **VU** — meters
6. **Mount / Connect** — stream target
7. **Remotes** (collapsible) — Accept, room code, R1–R4
8. **Settings / More** — preferences and session log

<!-- Screenshot: full Deck window, Remotes collapsed -->
![Deck main window](images/user-guide/02-deck-main.png)

### Audio devices (required before air)

Open **Settings → Audio**:

| Control | Purpose | Tip |
|---------|---------|-----|
| **Line Input (Deck)** | Music / mixer / app audio | USB interface, BlackHole, loopback |
| **Mic Input** | Your voice | Must be a **different** device from Line for ducking |
| **Monitor Output** | Headphones / cue | Pick a real output (not Off) before expecting cue audio |

Click **Save**. Meters update about half a second after a device change.

<!-- Screenshot: Settings → Audio -->
![Audio settings](images/user-guide/03-settings-audio.png)

### Mic mute

Large button under the mic section:

| Look | Meaning |
|------|---------|
| Mic glyph (normal) | Mic **live** / on air into the mix |
| Mic glyph with red **X** | Mic **muted** |

Click the button or press **Space** to toggle.

<!-- Screenshot: mic button live vs muted -->
![Mic mute button](images/user-guide/04-mic-mute.png)

---

## Streaming a show

1. Set **Line**, **Mic**, and **Monitor Output**.
2. Set **Line / Cart / Mic** faders to sensible levels.
3. Choose your **Mount** (ops will tell you which zone/slot).
4. Unmute mic when ready.
5. Press **Connect** (transport Play / connect control — green when streaming).
6. Optional: **Record** for a local post-mix capture (format in Settings → Record).

<!-- Screenshot: connected / on-air LCD or Connect lit -->
![Streaming connected](images/user-guide/05-streaming.png)

---

## Carts and ducking

### Carts

- **Right-click** cart 1–8 → assign **WAV, MP3, M4A, FLAC, or OGG**
- Per-cart **Level** in the cart setup (live while dragging if the cart is playing)
- **Click** or press **1–8** to fire (short fade; loop option in setup)

### Ducking

When you speak, music/carts can duck:

| Control | Role |
|---------|------|
| **Gate** | How loud you must be to duck |
| **Depth** | How far music drops |
| **Duck** indicator | Yellow when ducking is active |

Line and Mic must be separate devices or ducking will not behave correctly.

<!-- Screenshot: carts row + duck indicator -->
![Carts and ducking](images/user-guide/06-carts-duck.png)

---

## Headphones / monitor

- **Line + carts** go to **Monitor Output** only after you choose a real output in Settings.
- **Monitor** checkbox (near mic) — add **mic** into headphones when checked.
- Uncheck **Monitor** if you only want the deck bed (e.g. high Bluetooth mic latency).

<!-- Screenshot: Monitor checkbox and output choice -->
![Monitor controls](images/user-guide/07-monitor.png)

---

## Remotes (co-hosts)

Expand the **Remotes** section on Deck.

### Host steps

1. Tick **Accept** — Deck listens for remotes (LAN first, WAN relay if needed).
2. Note the **Code** (e.g. `AGT3B8`). Share that code with co-hosts.
3. Optional: **New code** to rotate the room code.
4. When a remote joins, they appear on **R1–R4** with level / mute.
5. **T** on a row injects a local 440 Hz test tone (no network) to check the fader path.

<!-- Screenshot: Remotes expanded with Accept + code + R1 live -->
![Remotes panel](images/user-guide/08-remotes-panel.png)

### What remotes hear (mix-minus)

Each remote hears:

- Deck line (music), carts, Deck mic  
- **Other** live remotes  
- **Not** their own voice  

Bluetooth headphones on the remote side may add latency; wired is preferred for talkback.

### More vs Remotes

Use **More** for the session info/log. Opening **More** should not cover the Remotes strip — if it does, collapse **More** or toggle **Remotes** once to refresh layout.

---

## TTNS Remote app

Co-hosts run **TTNS Remote**, not the full Deck.

### Launch

| Platform | Command / app |
|----------|----------------|
| macOS | Run `ttns-remote` from the handoff zip, or the copy inside `TTNS Deck.app/Contents/MacOS/` |
| Linux | `./run-ttns-remote.sh` from the unpacked linux folder |
| Windows | `bin\ttns-remote.exe` |

### Connect flow

1. Pick **mic** and **headphones** (or speakers).
2. Enter the room **code** from the Deck.
3. Press **Connect**.
4. Speak — you should appear as **R1** (etc.) on Deck and hear mix-minus.

<!-- Screenshot: Remote app idle -->
![Remote app before connect](images/user-guide/09-remote-idle.png)

<!-- Screenshot: Remote app connected -->
![Remote app connected](images/user-guide/10-remote-connected.png)

### LAN vs internet

| Situation | What happens |
|-----------|----------------|
| Same Wi‑Fi/LAN as Deck | UDP discovery + Opus/TCP (lowest latency) |
| Different network / internet | Falls back to WRX WebSocket relay (`wss://wrx.liveencode.com/ttns/ws`) |

Both ends need outbound HTTPS/WSS. Corporate firewalls that block WebSockets will prevent WAN dial-in.

---

## Telephone / core reach light

Deck (Remotes header) and Remote (near the slot badge) show a small **phone** indicator for reachability of `core.liveencode.com`:

| Colour | Meaning |
|--------|---------|
| **Grey** | Not checked yet / unknown |
| **Yellow** | Checking / intermittent |
| **Green** | Reachable |

This is a quick “can we see TTNS core from this machine?” cue — not a substitute for testing Accept + Connect.

<!-- Screenshot: phone LED grey / yellow / green -->
![Core reach phone LED](images/user-guide/11-reach-led.png)

---

## Settings cheat sheet

| Area | Typical DJ use |
|------|----------------|
| **Audio** | Line / Mic / Monitor devices |
| **Main** | Optional custom log path |
| **GUI** | UI scale (if available in this build) |
| **Record** | Local recording format |
| Server fields | Usually left alone — **Mount** presets fill them |

**Settings** and **More** sit in the lower control band with transport.

<!-- Screenshot: Settings window overview -->
![Settings overview](images/user-guide/12-settings.png)

---

## Logs and feedback

Deck writes a **session log** automatically. Path appears in **More**, for example:

| OS | Default log |
|----|-------------|
| macOS | `~/Library/Logs/TTNS Deck/ttns-deck.log` |
| Linux | `~/.local/state/ttns-deck/ttns-deck.log` |
| Windows | `%LOCALAPPDATA%\TTNS Deck\ttns-deck.log` |

When reporting bugs, send:

1. Platform (OS version + Apple Silicon / Intel / etc.)
2. Steps to reproduce
3. Expected vs actual
4. Session log (or More-panel copy)
5. Screenshot if UI-related

Do **not** post logs or zips publicly (mount names / credentials).

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| macOS “unidentified developer” | Right-click app → **Open** |
| Windows missing DLL | Unzip the **full** zip; run the `.bat` — do not copy only the `.exe` |
| No ducking | Line and Mic must be different devices |
| Remotes can’t find Deck on LAN | Same Wi‑Fi? Accept ticked? Try WAN (internet) path; check phone LED |
| Remote connects but no audio | Monitor output on Deck; Remote headphones device; unmute R-slot on Deck |
| Crackles / dropouts on Bluetooth | Prefer wired headphones on Remote; expect higher latency |
| More panel covers Remotes | Toggle Remotes / More once; update to this build |
| Meters stuck after device change | Wait for “Audio devices ready” in the log; re-pick devices |

---

## Screenshot checklist (for editors)

Add PNGs under `docs/images/user-guide/`:

| File | Shot |
|------|------|
| `01-install-package.png` | Unzipped handoff folder |
| `02-deck-main.png` | Full Deck, Remotes collapsed |
| `03-settings-audio.png` | Settings → Audio |
| `04-mic-mute.png` | Mic live + muted |
| `05-streaming.png` | Connected / on air |
| `06-carts-duck.png` | Carts + Duck lit |
| `07-monitor.png` | Monitor checkbox / output |
| `08-remotes-panel.png` | Accept + code + R1 |
| `09-remote-idle.png` | Remote before connect |
| `10-remote-connected.png` | Remote live |
| `11-reach-led.png` | Phone LED states |
| `12-settings.png` | Settings overview |

Placeholder images can be empty until tomorrow — keep the filenames stable so links above keep working.

---

## Related docs

| Doc | Audience |
|-----|----------|
| [`TESTER_FEEDBACK.md`](TESTER_FEEDBACK.md) | Short tester handoff |
| [`TTNS_DJ_GUIDE.md`](TTNS_DJ_GUIDE.md) | Compact DJ reference |
| [`REMOTE_DIALIN.md`](REMOTE_DIALIN.md) | Remote feature / branch notes |
| [`REMOTE_WAN.md`](REMOTE_WAN.md) | WRX relay deploy |
| [`RELEASE.md`](RELEASE.md) | Packaging and CI |
