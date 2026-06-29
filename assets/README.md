# TTNS assets

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
