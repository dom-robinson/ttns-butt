# TTNS Deck — binary releases

**Preliminary release:** `v0.1.16-ttns-pre.5` (tag on `ttns-mixer`)

Upstream BUTT is `0.1.16`; this fork adds the TTNS mixer UI and is versioned separately.

---

## Download

| Platform | CI artifact | Package contents |
|----------|-------------|------------------|
| **macOS Apple Silicon** | `ttns-deck-macos-arm64` | `TTNS Deck.app`, `ttns-deck-arm64-macos.zip` |
| **macOS Intel** | `ttns-deck-macos-x64` | `TTNS Deck.app`, `ttns-deck-x86_64-macos.zip` |
| **Linux x86_64** | `ttns-deck-linux-x64` | `ttns-deck-linux-x86_64.tar.gz` |
| **Windows x64** | `ttns-deck-windows-x64` | `ttns-deck-win64.zip` |

Each package includes `legal/` (or `Resources/legal/` on macOS) with `COPYING`,
`DISTRIBUTION_LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, and the Fraunhofer
FDK-AAC license text.

1. Open [Actions](https://github.com/dom-robinson/ttns-butt/actions) → latest workflow run on `ttns-mixer` or the release tag.
2. Download the artifact for your OS, **or** use the [GitHub Release](https://github.com/dom-robinson/ttns-butt/releases) (four platform archives only).
3. See [`TTNS_DJ_GUIDE.md`](TTNS_DJ_GUIDE.md) for install and first-run steps.

Tagged releases attach **only** the four platform zip/tar.gz files (not loose DLLs or
app bundle internals). For Dropbox handoff to DJs, run
`./scripts/package-dj-testers.sh /path/to/downloaded-ci-artifacts`.

---

## App icon

All packages use the TTNS logo:

| OS | Mechanism |
|----|-----------|
| **macOS** | `assets/ttns-deck.icns` → dock / Finder (`CFBundleIconFile`) |
| **Windows** | `assets/ttns-deck.ico` → exe icon via `src/resource.rc` |
| **Linux** | `assets/ttns-logo.png` in bundle; `.desktop` `Icon=ttns-deck` |
| **In-app** | Window icon + header logo from `assets/ttns-logo.png` |

Regenerate platform icons after logo changes:

```bash
./scripts/generate-icons.sh   # macOS: writes .icns + .ico; Linux/CI: .ico via ImageMagick/Pillow
```

Commit updated `assets/ttns-deck.icns` and `assets/ttns-deck.ico` so Windows and offline macOS builds stay consistent.

---

## Local packaging

From a clean tree with dependencies installed:

```bash
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh
```

| OS | Output |
|----|--------|
| macOS | `dist/macos/TTNS Deck.app`, `dist/macos/ttns-deck-$(uname -m)-macos.zip` |
| Linux | `dist/linux/ttns-deck-linux-$(uname -m).tar.gz` |
| Windows (MSYS2 MinGW64) | `dist/windows/ttns-deck-win64.zip` |

---

## CI (GitHub Actions)

Workflow: [`.github/workflows/build.yml`](../.github/workflows/build.yml)

| Job | Runner | Notes |
|-----|--------|-------|
| `build-macos` | `macos-latest` (arm64), `macos-15-intel` (x64) | Homebrew deps; `generate-icons.sh` + `build-macos-app.sh` |
| `build-linux` | `ubuntu-latest` | apt dev packages; `build-linux.sh` |
| `build-windows` | `windows-latest` + MSYS2 MinGW64 | Pillow for `.ico` if ImageMagick missing; bundles MinGW DLLs |

Pushes to `master` / `ttns-mixer` and tags `v*` trigger builds. Tags like `v0.1.16-ttns-pre.5` create a **pre-release** on GitHub with the four platform archives attached.

---

## macOS code signing (ops)

CI builds are **unsigned**. DJs: right-click → **Open** the first time.

For distribution outside the team:

```bash
codesign --force --deep --sign "Developer ID Application: …" "dist/macos/TTNS Deck.app"
xcrun notarytool submit dist/macos/ttns-deck-arm64-macos.zip --wait
```

---

## Known gaps (preliminary)

- Linux **arm64** tarball: build on arm64 runner or hardware (`build-linux.sh` already names arch).
- Windows: requires full zip (DLLs bundled); do not ship bare `ttns-deck.exe`. `v0.1.16-ttns-pre.2` fixes missing `libFLAC.dll` / FLTK 1.4 DLLs in older Windows packages.
- Intel macOS CI uses `macos-15-intel` (GitHub’s last x86_64 macOS image; retires ~2027).
