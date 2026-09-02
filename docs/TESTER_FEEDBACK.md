# TTNS Deck — tester feedback guide

**Build:** `0.1.16-ttns-remote.3` (Deck + Remote)

Thank you for testing. Please report issues with the **session log file** attached (see below).

Full walkthrough: [`USER_GUIDE.md`](USER_GUIDE.md).  
macOS blocked after download? [`MACOS_GATEKEEPER.md`](MACOS_GATEKEEPER.md).

---

## Install

| Platform | Deck | Remote |
|----------|------|--------|
| **macOS Apple Silicon** | `TTNS-Deck-…-macos-arm64.dmg` | `TTNS-Remote-…-macos-arm64.dmg` |
| **macOS Intel** | `TTNS-Deck-…-macos-x64.dmg` | `TTNS-Remote-…-macos-x64.dmg` |
| **macOS 12 Monterey** | `TTNS-Deck-…-macos-arm64-monterey12.dmg` | `TTNS-Remote-…-macos-arm64-monterey12.dmg` |
| **Linux** | `TTNS-Deck-…-linux-x64.tar.gz` | `TTNS-Remote-…-linux-x64.tar.gz` |
| **Windows** | `TTNS-Deck-…-windows-x64-setup.exe` | `TTNS-Remote-…-windows-x64-setup.exe` |

Send the **.dmg or .exe file**, never a raw `.app` (Dropbox shows a `Contents` folder).

### macOS Gatekeeper (important)

If you see **“Apple could not verify…”** with **Move to Bin**:

1. Click **Done** (do **not** Move to Bin).
2. **System Settings → Privacy & Security** → **Open Anyway**.

Or in Terminal: `xattr -cr` on the `.app` (details in `MACOS_GATEKEEPER.md`).

First Deck launch creates `~/.buttrc` (or `%USERPROFILE%\.buttrc` on Windows).

---

## Session log (please attach when reporting bugs)

TTNS Deck writes a **session log** automatically. On startup the path is shown in the **More** panel, e.g.:

```
Session log: /Users/you/Library/Logs/TTNS Deck/ttns-deck.log
```

| OS | Default log path |
|----|------------------|
| **macOS** | `~/Library/Logs/TTNS Deck/ttns-deck.log` |
| **Linux** | `~/.local/state/ttns-deck/ttns-deck.log` |
| **Windows** | `%LOCALAPPDATA%\TTNS Deck\ttns-deck.log` |

The log records timestamped events: audio device open/close, connect/disconnect, errors, and TTNS audio path messages. **Do not post logs publicly** — they may mention mount names.

You can also copy text from the in-app **More** panel (bottom-right) and paste into email/Slack.

Optional: set a custom log path in **Settings → Main → Log file**.

---

## What to test

1. Pick **Line** and **Mic** devices (Settings → Audio) — must be different for ducking. Try **Refresh devices** after plugging in a USB interface.
2. Set levels (Line, Cart, Mic), try a cart (right-click to assign MP3/WAV).
3. Choose **Mount**, press **Connect**, brief stream test if scheduled.
4. **Remotes:** expand Remotes → **Accept** → note room code → join from **TTNS Remote** on another machine.
5. **PTT:** with a remote connected, tick **PTT** — audience should keep hearing unducked music; host and guests still hear each other. Untick to put voices back on-air.
6. Check the phone reach LED (grey / yellow / green) for `core.liveencode.com`.
7. Note anything odd: crackling, crash, wrong levels, remote dropouts, layout glitches.

---

## Send feedback to TTNS ops

Include:

1. **Platform** (macOS version + Apple/Intel, Linux distro, Windows version)
2. **What you did** (steps to reproduce)
3. **What happened** vs what you expected
4. **Session log file** (or More-panel copy)
5. **Screenshot** if UI issue

Do not share build zips or config files outside the TTNS team (Icecast passwords).
