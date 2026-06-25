# TTNS BUTT

TTNS-specific fork of [BUTT](https://github.com/romansavrulin/butt) (Broadcast Using This Tool) — a cross-platform Icecast/Shoutcast streaming client for live DJ / presenter use.

| | |
|---|---|
| **Upstream** | [romansavrulin/butt](https://github.com/romansavrulin/butt) (mirror of [SourceForge butt](https://sourceforge.net/projects/butt/)) |
| **This fork** | [dom-robinson/ttns-butt](https://github.com/dom-robinson/ttns-butt) |
| **Base version** | butt 0.1.16 |
| **License** | GPL-2.0 (`COPYING`) |

The upstream user manual is in `README`. TTNS fork notes and changelog: `README.TTNS.md`, `CHANGELOG.md`.

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

## Planned TTNS customizations

- [ ] TTNS branding (app name, icons, about dialog)
- [ ] Pre-configured Icecast/Shoutcast server profiles for TTNS
- [ ] Default stream metadata (station name, URL, genre)
- [ ] Simplified first-run setup for TTNS DJs
- [ ] macOS build/signing pipeline for distribution to presenters

## Linux / Windows

See `INSTALL` and upstream `README`. This fork has not yet been validated on those platforms.
