# Changelog

All notable TTNS-specific changes to this fork are documented here.
Upstream BUTT release notes remain in `NEWS`.

## [0.1.16-ttns] — TTNS Deck UI polish (2026-06-26)

### Added

- **`Fl_Ttns_Mic_Button`** — large LIVE/MUTED mic toggle (green/red icon) below About
- **`Fl_Ttns_Check_Button`** — black/red square checkboxes (label left, box right)
- **`Fl_Ttns_Fader`**, **`Fl_Ttns_Cart_Button`**, **`Fl_Ttns_Transport_Button`**, **`Fl_Ttns_Border_Button`** — TTNS-themed controls
- **`ttns_theme`** — terminal green + red accent palette across main and Advanced windows
- Single **Mount** dropdown (`ttnszone 1-1` … `4-5`) replaces separate Zone/Slot pickers
- Deck/Mic device pickers moved to **Settings → Audio**
- LCD status panel: taller display, red rounded frame, full-height divider
- VU meters embedded in Line/Mic fader tracks
- Space bar toggles mic mute (same as clicking the mic button)
- Collapsed log panel; Settings button above More (bottom-right)

### Changed

- Window title **TTNS Deck**; config window **TTNS Advanced**
- Monitor row: **Monitor** and **Mic to Mon** checkboxes, right-aligned with fader row
- Mic mute: removed checkbox and momentary space-hold behaviour — one obvious toggle only
- Duck indicator: label + yellow LED when ducking active
- Cart hotkeys 1–8; config auto-save on quit via `[ttns]` section

### Fixed

- Fader track visibility and red drag handles
- Settings tab highlight (red on black, not green-on-green)
- Space key no longer fights mic mute checkbox state
- Transport/LCD layout and text overflow on idle status

## [Unreleased] — Phase E complete (cross-platform distribution)

### Added

- **`ttns_paths`** — finds `data/` and `assets/` beside exe, app bundle, or `/usr/share/ttns-deck/`
- **About** dialog + window icon from TTNS logo
- **`docs/TTNS_DJ_GUIDE.md`** — presenter guide (macOS, Linux, Windows)
- **`scripts/generate-icons.sh`** — `.icns` + `.ico` from logo
- **`scripts/build-linux.sh`**, **`scripts/build-windows.sh`**, **`scripts/build-release.sh`**
- **`.github/workflows/build.yml`** — CI artifacts for macOS, Linux, Windows
- `make install` → `ttns-deck` binary + `/usr/share/ttns-deck/` data on Linux
- Package renamed to **ttns-deck** `0.1.16-ttns` in autotools metadata

### Changed

- macOS app bundle: dock icon, microphone usage string, zip artifact per arch
- Windows `resource.rc` → `assets/ttns-deck.ico`
- Startup log messages say **TTNS Deck**

## [Unreleased] — TTNS mixer Phases B + C

### Added

- **Mic ducking** — line+cart bus ducks when mic exceeds threshold (default −12 dB depth, 30 ms attack, 300 ms release)
- **`cart_player.cpp`** — 8 cart slots, WAV/MP3/FLAC/OGG, auto-resample to stream rate
- Cart playback: click to play, right-click (or ⌘/Ctrl+click) to assign file
- 300 ms fade in/out on carts; loop-latch mode via `cartN_mode = 1` in config
- `[ttns]` config: `duck_*` fields and `cart1_path` … `cart8_path` (+ label, mode)

## [Unreleased] — TTNS mixer (Phase 0 + A)

### Added

- `src/ttns_audio.cpp` — stereo mix helper (mic + line buses)
- `src/ttns_zones.cpp` — loads `data/ttns-zones.json`; applies Icecast mount/password and stream metadata on Connect
- `src/ttns_ui.cpp` — TTNS panel: logo, faders, zone connect
- `[ttns]` config section: devices, gains, zone, slot, monitor flags
- Dual PortAudio capture when mic and line use different devices

## [Unreleased] — TTNS macOS build

### Added

- `README.TTNS.md`, `docs/TTNS_DEVELOPMENT_PLAN.md`, `data/ttns-zones.json`, `assets/ttns-logo.png`
- Homebrew path detection in `configure.ac`; macOS Objective-C++ build support

### Fixed

- `port_audio.cpp`: libsamplerate 0.2.x compatibility; linker `OBJCXXFLAGS` issue
