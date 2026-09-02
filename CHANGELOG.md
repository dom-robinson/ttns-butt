# Changelog

All notable TTNS-specific changes to this fork are documented here.
Upstream BUTT release notes remain in `NEWS`.

## [0.1.16-ttns-remote.3] — Standalone Remote installers

**Tag:** `v0.1.16-ttns-remote.3` (on `master`)

### Added

- Separate **TTNS Remote** installers for every platform (same naming as Deck):
  - macOS `.dmg` (arm64, Intel, Monterey 12)
  - Windows `setup.exe`
  - Linux `tar.gz`

Deck and Remote are no longer bundled on one Mac disk image. Co-hosts get the Remote installer only.

---

## [0.1.16-ttns-remote.2] — Device hotplug + PTT remotes

**Tag:** `v0.1.16-ttns-remote.2` (on `master`)

### Added

- **Refresh devices** in Settings → Audio — rescan USB/virtual devices without restarting Deck
- **PTT** on the Remotes header — host and guests talk off-air; program (line/carts) is not ducked
- Line/Mic remembered **by name** (same idea as Monitor) so device-list reshuffles do not silently pick the wrong box

### Fixed

- Unplug/replug: if the stream dies, Deck rescans PortAudio and reopens the named Line/Mic/Monitor
- Line/Mic/Monitor can be changed while connected (short mix gap; Icecast stays up)

---

## [0.1.16-ttns-remote.1] — Crew release (VB-Cable + one-click packages)

**Tag:** `v0.1.16-ttns-remote.1` (on `master`)

### Fixed

- Line input no longer crashes when a virtual cable (VB-Cable, loopback) delivers **NULL** audio while the music player is stopped — treated as silence so carts, mic, and the Icecast stream keep running
- If PortAudio aborts the input stream (device drop), Deck **reopens** the same devices without clearing Settings / carts

### Added

- macOS **.dmg** (drag TTNS Deck + TTNS Remote to Applications)
- Windows **setup.exe** (Start Menu / desktop shortcuts; no admin required)
- `scripts/build-macos12-app.sh` — Apple Silicon build for **macOS 12 Monterey** (Homebrew bottles on current macOS cannot run there)

### Changed

- DJ downloads: send the **.dmg** or **setup.exe** file. Do not send a raw `.app` via Dropbox/email (it arrives as a `Contents` folder).

---

## [0.1.16-ttns-remote-dev.4] — Bundle Homebrew dylibs into macOS apps (dev)

**Branch:** `feature/remote-dial-in`

### Fixed

- macOS Deck/Remote now **embed Frameworks** (portaudio, fltk, curl, …) so they launch on machines without Homebrew
- Ad-hoc codesign in a clean temp tree (Desktop `com.apple.provenance` was breaking signing)

---

## [0.1.16-ttns-remote-dev.3] — macOS Remote.app + Gatekeeper notes (dev)

**Branch:** `feature/remote-dial-in`

### Changed

- **TTNS Remote** ships as **TTNS Remote.app** (not a bare Terminal binary)
- Docs: [`docs/MACOS_GATEKEEPER.md`](docs/MACOS_GATEKEEPER.md) for Sequoia “could not verify” / Open Anyway
- Helper: `scripts/macos-clear-quarantine.sh`

---

## [0.1.16-ttns-remote-dev.2] — Crew test UI + packaging (dev)

**Branch:** `feature/remote-dial-in` (does not replace `v0.1.16-ttns-pre.7`)

### Changed

- Transport / VU / Settings / More layout polish; shorter LCD; More no longer covers Remotes
- Mic mute glyph (no LIVE/MUTED text); phone reach LED for `core.liveencode.com`
- Remotes header: code flush left of New code; Accept + reach indicator
- Docs: [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) (Deck + Remote, screenshot placeholders)
- Packager version strings aligned to this build; CI also builds `feature/remote-dial-in`

---

## [0.1.16-ttns-remote-dev.1] — Remote dial-in foundation (dev)

**Branch:** `feature/remote-dial-in` (does not replace `v0.1.16-ttns-pre.7`)

### Added

- 4-slot remote co-host bus in the mix with per-slot mix-minus return paths
- Operator UI: Accept remotes, room code, R1–R4 faders/mutes, local test-tone inject
- LAN Opus/TCP + UDP discovery; WAN WebSocket relay via WRX
- **TTNS Remote** binary (`ttns_remote` → `ttns-remote`)
- Docs: [`docs/REMOTE_DIALIN.md`](docs/REMOTE_DIALIN.md), [`docs/REMOTE_WAN.md`](docs/REMOTE_WAN.md)

---

## [0.1.16-ttns-pre.7] — Safe audio device switching (2026-07-03)

**Tag:** `v0.1.16-ttns-pre.7`

### Fixed

- **All platforms** — changing Line/Mic/Monitor devices no longer crashes; meters follow the selected device
- **macOS** — debounced reopen via `snd_reinit()`; mic-only path avoids tearing down deck input when only mic changes
- **macOS** — no speaker feedback on cold start (monitor off until Monitor Output is chosen); mono mic capture for virtual devices (SplitCam, Zoom)
- **All platforms** — monitor output changes handled separately from input device reopen; meter peaks reset on stream stop

### Changed

- New configs default `mic_monitor = 0` and `mic_monitor_mute = 1` (monitor playback off until explicitly enabled)
- Save applies pending audio settings; device dropdowns apply after ~0.45s debounce

---

## [0.1.16-ttns-pre.6] — macOS device change double-free (2026-07-02)

**Tag:** `v0.1.16-ttns-pre.6`

### Fixed

- **macOS / all platforms** — double-free crash changing audio device (`monitor_mix_buf` not nulled after `free`)
- **macOS** — brief pause after closing PortAudio streams before reopen (CoreAudio)

---

## [0.1.16-ttns-pre.5] — Windows audio device change crash (2026-07-01)

**Tag:** `v0.1.16-ttns-pre.5`

### Fixed

- **Windows** — crash when changing Line/Mic input device or importing config with edited device index
- Ring buffers (`rec_rb`, `stream_rb`) now torn down before re-init on device change
- Audio reopen deferred off FLTK choice callback; VU meter paused during teardown
- Config import stops open streams before re-enumerating devices
- Device dropdowns cleared before refill on import

---

## [0.1.16-ttns-pre.4] — GitHub release packaging fix (2026-07-01)

**Tag:** `v0.1.16-ttns-pre.4`

### Fixed

- **GitHub Release** — attach only the four platform archives; avoid duplicate asset upload failures from merged CI trees
- **`scripts/stage-release-packages.sh`** — collect release zips from CI artifacts or local `dist/`
- **`scripts/package-dj-testers.sh`** — renamed DJ handoff packages; accepts CI artifact directory

---

## [0.1.16-ttns-pre.3] — Licensing and distribution compliance (2026-07-01)

**Tag:** `v0.1.16-ttns-pre.3`

### Added

- **`docs/LICENSING.md`** — GPL obligations, third-party summary, branding note
- **`docs/DISTRIBUTION_LICENSE.txt`** — short notice shipped in binary packages
- **`docs/THIRD_PARTY_NOTICES.md`** — attribution for linked libraries
- **`docs/licenses/fdk-aac-LICENSE.txt`** — full Fraunhofer FDK-AAC text (binary redistribution requirement)
- **`scripts/copy-distribution-licenses.sh`** — stages `legal/` into macOS, Linux, and Windows packages

### Changed

- Release packages now include `legal/` (`Resources/legal/` on macOS) with GPL + third-party notices
- **`assets/README.md`** — TTNS logo copyright documented

---

## [0.1.16-ttns-pre.2] — Windows DLL bundling (2026-07-01)

**Tag:** `v0.1.16-ttns-pre.2`

### Fixed

- **Windows** — bundle FLTK 1.4, `libFLAC.dll`, and transitive MinGW deps; CI fails if any import DLL is missing

---

## [0.1.16-ttns-pre.1] — Preliminary release (2026-06-29)

**Tag:** `v0.1.16-ttns-pre.1` — first binary release candidate for DJ testing.

### Added

- **Cart deck fader** — master level for all carts (separate from Line fader); still ducked with line
- **Per-cart level** in cart setup — live preview while dragging; **Cancel** restores previous trim
- **MP3 / M4A carts on macOS** — AVFoundation decode (`cart_loader_mac.mm`); no LAME crash on hot MP3s
- Cart file picker filter fixed for macOS (`wav`, `mp3`, `m4a`, `flac`, `ogg`)
- **Monitor** checkbox under mic LIVE/MUTED button (compact layout; line/deck always in headphones)
- **`docs/RELEASE.md`** — binary download, CI, icons, signing notes
- CI: macOS **arm64** + **Intel** (`macos-latest`, `macos-15-intel`), Linux, Windows artifacts
- GitHub Release on version tags (pre-release)

### Changed

- Mix bus: `line × line_gain + cart × cart_gain` (each ducked independently)
- Cart setup dialog: `Fl_Ttns_Fader` + double-buffered window (no slider ghosting)
- Window height reduced — removed spare row below Depth fader
- Mic monitor: **Monitor** adds mic to headphones only; line always audible (Bluetooth delay use case)

### Fixed

- **Bus error** loading MP3 carts on macOS (SIGBUS in LAME `hip_decode`)
- Mic ring buffer sync, monitor teardown race, device-change crashes
- Ducking gated when mic muted; mic meter shows pre-fader level when off-air

### Distribution

- App icon from TTNS logo: `.icns` (macOS), `.ico` (Windows), in-window PNG
- See [`docs/RELEASE.md`](docs/RELEASE.md) and [`docs/TTNS_DJ_GUIDE.md`](docs/TTNS_DJ_GUIDE.md)

---

## [0.1.16-ttns.1] — Audio metering and settings layout (2026-06-29)

### Fixed

- **Fragmented mic audio** — mic and line inputs sync through a ring buffer; monitor uses an output callback
- **Monitor UI** — mic monitor always on with separate Line/Mic devices; monitor output picker in Settings
- Stack buffer overflow on device change; safe teardown; dual mic capture via dedicated PortAudio callback
- Post-fader VU on Line/Mic faders; Settings tab layout and checkbox alignment

### Changed

- Startup log shows TTNS audio path (dual / shared / line-only)

---

## [0.1.16-ttns] — TTNS Deck UI polish (2026-06-26)

TTNS-themed mixer UI: logo, faders, carts, mount dropdown, mic LIVE/MUTED button, ducking, 8 cart slots, cross-platform paths, About dialog, build scripts, CI workflow foundation.

See git history before `0.1.16-ttns-pre.1` for full Phase A–E detail.
