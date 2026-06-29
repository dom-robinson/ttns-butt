# TTNS BUTT — development plan

High-level roadmap for TTNS-specific features. Base: **butt 0.1.16** fork with working macOS autotools build.

## Goals

| # | Feature | DJ outcome |
|---|---------|------------|
| 1 | **Dual faders** | Separate mic and line level controls on a simplified main UI |
| 2 | **Mic ducking** | Line drops when mic is active so voice sits clearly on top |
| 3 | **Cart deck** | 8 buttons for short WAV jingles (one-shot or latch loop, 300 ms fades) |
| 4 | **TTNS zones** | Pick show slot → Icecast connection and metadata filled in automatically |

## Current architecture (upstream)

```
Single PortAudio device → global gain → ring buffer → encode → Icecast/Shoutcast
```

No mixing, ducking, or file playback. UI is FLUID (`flgui.fl`); config is INI (`.buttrc`).

## Target architecture

```
Mic PA stream (USB / built-in / Bluetooth) ──► mic fader ────────────────┐
                                                                        │
Line PA stream (USB mixer OR app audio*) ──► line fader ──┐           │
                                                           ├──► line+cart bus (stereo)
Cart WAV slots (×8) ──────────────────────► cart mixer ──┘           │
                                                                        ▼
                                                              mic ducking (when mic active)
                                                                        ▼
                                                              master mix (stereo)
                                                                        ▼
                                              ┌─────────────────────────┴─────────────────────────┐
                                              ▼                                                   ▼
                                         encode → Icecast                              record (post-mix)
                                              ▼
                                    optional mic monitor (local headphones;
                                    muteable — important for Bluetooth latency)
```

\* **App audio** (Apple Music, Spotify, etc.): selectable line source on macOS via loopback
virtual device (e.g. BlackHole / Loopback) or future ScreenCaptureKit path — DJ routes player
output to that device in system settings.

**Real-time mix point:** `snd_callback()` in `src/port_audio.cpp`  
**UI:** new TTNS panel in `flgui.fl` + callbacks in `fl_callbacks.cpp`  
**Config:** extend `cfg.h` / `cfg.cpp`; TTNS presets in `data/ttns-zones.json` (or API later)

---

## Decisions (locked in)

### 1 — Inputs

| Bus | Typical source | Selection |
|-----|----------------|-----------|
| **Line** (stereo) | USB audio interface / external mixer | PortAudio device picker |
| **Line** (alt) | Local app playback (Apple Music, Spotify, …) | Selectable “app audio” / loopback device in line source menu |
| **Mic** (mono or stereo) | Separate device: USB mic, laptop built-in, or Bluetooth | Independent PortAudio device picker |

- Mic and line are **always separate capture streams** (not L/R split on one cable).
- Both buses are **stereo** at the mix layer; mic may be mono-capable hardware.
- **Mic monitor:** optional local headphone/speaker feed of the mic bus, with a **mute monitor** toggle (Bluetooth latency makes monitoring undesirable for some DJs).

### 2 — Carts

- Carts mix into the **line bus** (same stereo channel as music/line input).
- The combined **line + cart** bus is **subject to mic ducking** when the mic is active.

### 3 — Recording

- **Post-mix only** — recording taps the same signal sent to Icecast (after ducking and faders).
- Stream and record always share one mixed output; no separate pre-mix record path.

### 4 — Distribution

- **Private git repository** — source and `data/ttns-zones.json` are not public.
- **DJ delivery:** compiled binaries only (macOS first, other platforms later).

### Icecast presets

Bundled in [`data/ttns-zones.json`](../data/ttns-zones.json):

| Setting | Value |
|---------|--------|
| Host | `decks.thethursdaynightshow.com` |
| Port | `8080` |
| User | `source` |
| Type | Icecast |

**20 mounts:** `ttnszone{1-4}_{1-5}` — each with its own source password.

**Stream metadata (all mounts):**

| Field | Value |
|-------|--------|
| Description | `Live Now on TheThursdayNightShow and on TTNS.FM` |
| Genre | `eclectic` |

**Zone picker (v1):** manual **Zone** (1–4) + **Slot** (1–5). Scheduler auto-mapping deferred.

### Still open

| Question | Options | Impact |
|----------|---------|--------|
| Simplified UI | Replace main window vs TTNS mode toggle | `flgui.fl` scope |
| App-audio MVP | Virtual loopback device only vs built-in app picker | Phase A scope on macOS |

---

## Phases

### Phase 0 — Foundation (½ day)

*Prep before feature work.*

- [ ] Create `ttns` git branch for feature work
- [ ] Document test setup: device(s), test Icecast mount, sample WAV carts
- [ ] Resolve remaining open questions (zones data, UI shell — see Decisions)
- [ ] Add minimal `ttns_audio.h` / mixer module skeleton (no behaviour yet)

**Exit:** branch ready, test checklist, decisions logged.

---

### Phase A — Dual input + simplified UI (1–2 weeks)

*Separate mic and line faders; TTNS-facing main window.*

- [ ] **Two independent PortAudio input streams:**
  - **Line** (stereo): USB mixer/interface *or* loopback device for app audio (Music/Spotify)
  - **Mic**: USB mic, built-in laptop mic, or Bluetooth mic (separate device)
- [ ] Line source selector: hardware device list + “application audio (loopback)” entry with setup hint
- [ ] Replace single `cfg.main.gain` with `mic_gain` + `line_gain` (dB → linear)
- [ ] Resample/sync if mic and line devices run at different rates or drift (buffer + libsamplerate)
- [ ] Mix: `line_bus = line_fader(line_in) + cart` (cart = 0 until Phase C); `out = mic_fader(mic_in) + line_bus` (ducking added in Phase B)
- [ ] **Post-mix tap:** single mixed stereo buffer → `stream_rb` and `rec_rb` (same signal to Icecast and disk)
- [ ] Split VU: mic + line (and later cart activity indicator)
- [ ] **Mic monitor:** optional PA output stream or tap to default output; **Monitor mute** toggle
- [ ] **TTNS simplified UI** — new window or simplified main layout:
  - **TTNS logo** in header (`assets/ttns-logo.png`)
  - Mic fader, line fader, line source + mic source pickers
  - Monitor on/off (or monitor level + mute)
  - Connect / disconnect, record (unchanged behaviour, post-mix)
  - Hide advanced BUTT settings behind “Advanced”
- [ ] Persist levels and device IDs in `.buttrc` `[ttns]` section

**Key files:** `port_audio.cpp`, `cfg.h`, `cfg.cpp`, `flgui.fl`, `fl_callbacks.cpp`, `fl_funcs.cpp`, `vu_meter.cpp`

**Exit:** DJ can balance mic and line live; stream hears the mix.

---

### Phase B — Mic ducking (~3–5 days)

*Depends on Phase A (two buses).*

- [ ] Duck **line + cart bus** (combined stereo music/jingle channel) when mic peak exceeds threshold
- [ ] UI faders/controls:
  - Duck depth (how much line drops, dB)
  - Threshold (minimum mic level to trigger)
  - Attack / release (ms) — start with fixed sensible defaults if needed
- [ ] Ducking runs in `snd_callback()` (no allocations, no locks)
- [ ] Optional: duck indicator on UI when ducking is active
- [ ] Config: `[ttns]` duck_* fields

**Exit:** talking on mic clearly reduces line level; release restores line when mic is quiet.

---

### Phase C — Cart deck (1–2 weeks)

*Carts on the line bus; ducked with line when mic is active.*

- [ ] **8 cart slots** — file path per slot, label on button
- [ ] Load WAV (16-bit PCM; resample to stream rate if needed)
- [ ] **Modes per slot:**
  - **One-shot:** play start→end, 300 ms fade in/out, stop
  - **Loop latch:** fade in on press, loop until press again, 300 ms fade out on release
- [ ] Cart audio sums into **line bus** (stereo); ducking (Phase B) applies to line+cart together
- [ ] Background: decode ahead into per-cart ring buffer; callback only pulls samples
- [ ] UI: 8 buttons + right-click or long-press to assign WAV file
- [ ] Config: `[ttns]` cart paths + mode per slot

**Key files:** new `cart_player.cpp` / `cart_player.h`, `port_audio.cpp`, `flgui.fl`

**Exit:** DJ can fire jingles without stopping the stream; fades are smooth.

---

### Phase D — TTNS zones & show picker (~1 week)

*Icecast presets defined — see `data/ttns-zones.json`.*

- [ ] Loader for `data/ttns-zones.json` → in-memory zone/slot table
- [ ] UI on simplified main window:
  - **Zone** dropdown (1–4)
  - **Slot** dropdown (1–5)
  - Display selected mount name (e.g. `ttnszone2_3`)
- [ ] On Connect → set `cfg.srv[]` and `cfg.icy[]` from JSON (host, port, user, mount, password, description, genre)
- [ ] Hide manual server editing for TTNS builds (keep under Advanced)
- [ ] Later (optional): scheduler API maps show time → zone/slot

**Exit:** DJ picks zone + slot and hits Connect — no manual server setup.

---

### Phase E — Polish & distribution ✅

- [x] TTNS branding: logo, **TTNS Deck** title, window icon, **About** dialog
- [x] Cross-platform resource paths (`ttns_paths`)
- [x] macOS `.app` + zip (`scripts/build-macos-app.sh`, dock `.icns`)
- [x] Linux tarball + `make install` (`scripts/build-linux.sh`)
- [x] Windows zip + DLL bundle (`scripts/build-windows.sh`, MSYS2)
- [x] CI: macOS / Linux / Windows (`.github/workflows/build.yml`)
- [x] DJ guide: `docs/TTNS_DJ_GUIDE.md`
- [ ] Apple **notarization** / Developer ID signing (ops step — documented in README.TTNS.md)

---

## Dependency graph

```
Phase 0
   └──► Phase A (dual faders + UI)
            └──► Phase B (ducking)
            └──► Phase C (cart deck)     ── can parallel after A if staffed
   Phase D (zones) ── can start data model in parallel; UI needs A's simplified shell
   Phase E ── continuous
```

**Suggested order:** 0 → A → B → C → D → E  
**Tomorrow:** Phase 0 + start Phase A.

---

## Test checklist (grow each phase)

- [ ] Stream connects to test Icecast mount
- [ ] Line: USB mixer and loopback/app-audio device both work as line source
- [ ] Mic: USB, built-in, and Bluetooth devices selectable independently from line
- [ ] Mic monitor audible when enabled; mute silences monitor without affecting stream
- [ ] Mic-only / line-only / both at correct relative levels
- [ ] Ducking: line **and carts** drop when mic spoken, recover when silent
- [ ] Each cart: one-shot completes; loop latch toggles; fades ~300 ms
- [ ] **Record file matches Icecast content** (post-mix, including ducking)
- [ ] Show picker fills correct server fields and connects
- [ ] Settings survive restart (`.buttrc`)
- [ ] No xruns / dropouts under normal DJ use (30+ min soak)

---

## Repo conventions

- Work on feature branch; merge to `master` per completed phase
- Update `CHANGELOG.md` and `README.TTNS.md` at end of each phase
- Regenerate `flgui.cpp` from FLUID after `.fl` edits (`fluid -c flgui.fl` or FLUID GUI)
- Rebuild: `./configure && make` from repo root

---

## References

| Doc / path | Purpose |
|------------|---------|
| `README.TTNS.md` | Fork overview, build instructions |
| `CHANGELOG.md` | TTNS-specific changes |
| `src/port_audio.cpp` | Audio callback & encode threads |
| `src/cfg.h` | Config structures |
| `data/ttns-zones.json` | Icecast host, zones, mounts, source passwords |
| `assets/ttns-logo.png` | Main UI logo (to be supplied) |
| `src/FLTK/flgui.fl` | UI layout (FLUID source) |