# TTNS BUTT

TTNS-specific fork of [BUTT](https://github.com/romansavrulin/butt) (Broadcast Using This Tool) — a cross-platform Icecast/Shoutcast streaming client.

## Upstream

| Remote   | Repository |
|----------|------------|
| `origin` | https://github.com/dom-robinson/ttns-butt (this fork) |
| `upstream` | https://github.com/romansavrulin/butt |

Upstream is a mirror of the official BUTT project on [SourceForge](https://sourceforge.net/projects/butt/). This tree is based on **butt 0.1.16**.

To pull upstream changes:

```bash
git fetch upstream
git merge upstream/master
```

## Build (macOS)

Install dependencies with Homebrew:

```bash
brew install fltk portaudio lame libvorbis libogg flac opus libsamplerate fdk-aac pkg-config
```

From the project root:

```bash
./configure
make
```

The Xcode project under `xcode/` can also be used on macOS.

See `INSTALL` and `README` for Linux and Windows builds.

## Planned TTNS customizations

- [ ] TTNS branding (app name, icons, about dialog)
- [ ] Pre-configured Icecast/Shoutcast server profiles for TTNS
- [ ] Default stream metadata (station name, URL, genre)
- [ ] Simplified first-run setup for TTNS DJs
- [ ] macOS build/signing pipeline for distribution to presenters

Track implementation in this repo; upstream manual remains in `README`.

## License

GPL-2.0 — see `COPYING`.
