# TTNS assets

## Copyright

`ttns-logo.png`, `ttns-deck.icns`, `ttns-deck.ico`, and derived platform icons
are **copyright The Thursday Night Show**. They are not licensed under GPL-2.0
(only the butt-derived *program code* is). Official TTNS Deck builds may include
these assets for TTNS presenters; do not reuse the TTNS mark outside TTNS
without permission.

Upstream butt UI bitmaps in `src/xpm/` and `usr/share/pixmaps/` remain under
the same GPL-2.0 as the butt source code.

## `ttns-logo.png`

Embossed TTNS logo on black (307×282 PNG, RGBA). Used for:

- Main UI header and About dialog
- Window icon (`ttns_set_window_icon`)
- Source for platform app icons

## Platform icons

| File | Use |
|------|-----|
| `ttns-deck.icns` | macOS dock / Finder (`TTNS Deck.app`) |
| `ttns-deck.ico` | Windows executable icon (`src/resource.rc`) |
| `ttns-deck.desktop` | Linux desktop entry (`Icon=ttns-deck`) |

Regenerate after replacing the logo:

```bash
./scripts/generate-icons.sh
```

Commit updated `.icns` and `.ico` so CI and offline builds stay in sync.
