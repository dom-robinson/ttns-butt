# TTNS BUTT

TTNS-specific fork of [BUTT](https://github.com/romansavrulin/butt) (Broadcast Using This Tool) — a cross-platform Icecast/Shoutcast streaming client for live DJ / presenter use.

| | |
|---|---|
| **Upstream** | [romansavrulin/butt](https://github.com/romansavrulin/butt) (mirror of [SourceForge butt](https://sourceforge.net/projects/butt/)) |
| **This fork** | [dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt) |
| **Base version** | butt 0.1.16 |
| **License** | GPL-2.0 (`COPYING`) |
| **Distribution** | Private repo; compiled binaries only for TTNS DJs |

The upstream user manual is in `README`. TTNS fork notes: `README.TTNS.md`, `CHANGELOG.md`, development plan: [`docs/TTNS_DEVELOPMENT_PLAN.md`](docs/TTNS_DEVELOPMENT_PLAN.md).

## Git remotes

| Remote | Repository |
|--------|------------|
| `origin` | https://github.com/dom-robinson/ttns-butt |
| `upstream` | https://github.com/romansavrulin/butt |

Pull upstream changes:

```bash
git fetch upstream
git merge upstream/master
```

## Build (macOS)

### Dependencies

```bash
brew install fltk portaudio lame libvorbis libogg flac opus libsamplerate fdk-aac pkg-config autoconf automake libtool
```

### Compile

```bash
./configure
make
```

Binary: `src/butt`

`./configure` detects Homebrew on Apple Silicon (`/opt/homebrew`) and Intel Macs (`/usr/local`). On macOS, C++ sources build as Objective-C++ (required for Cocoa window helpers in FLTK).

### Run

```bash
./src/butt
```

First launch creates `~/.buttrc`. See upstream `README` for streaming setup.

### Alternative: Xcode

An Xcode project lives under `xcode/`. It bundles older static libraries and may need deployment-target updates on current Xcode; **autotools is the supported TTNS build path on macOS for now.**

## Development

Regenerate autotools after editing `configure.ac` or `Makefile.am`:

```bash
autoreconf -fi
./configure
make clean && make
```

## TTNS streaming (planned)

Connection presets: [`data/ttns-zones.json`](data/ttns-zones.json)

| | |
|---|---|
| **Server** | `decks.thethursdaynightshow.com:8080` (Icecast, user `source`) |
| **Mounts** | `ttnszone{1-4}_{1-5}` (manual zone + slot picker) |
| **Description** | Live Now on TheThursdayNightShow and on TTNS.FM |
| **Genre** | eclectic |

Logo: [`assets/ttns-logo.png`](assets/ttns-logo.png)

## Planned TTNS customizations

See [`docs/TTNS_DEVELOPMENT_PLAN.md`](docs/TTNS_DEVELOPMENT_PLAN.md) for the full roadmap.

- [ ] TTNS logo on main UI (`assets/ttns-logo.png`)
- [ ] Dual mic/line faders, mic ducking, 8-button cart deck
- [ ] Zone/slot picker wired to `data/ttns-zones.json`
- [ ] macOS signed build for presenter distribution

See `INSTALL` and upstream `README`. This fork has not yet been validated on those platforms.
