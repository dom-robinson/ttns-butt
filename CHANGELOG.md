# Changelog

All notable TTNS-specific changes to this fork are documented here.
Upstream BUTT release notes remain in `NEWS`.

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
