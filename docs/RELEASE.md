# TTNS Deck — binary releases

**Current tag:** `v0.1.16-ttns-remote.1` (on `master`) — Deck + Remote, VB-Cable idle fix, one-click Mac/Windows packages.

Packaging version is `packaging/VERSION` (kept out of the source root so it does not collide with C++ `<version>` on macOS). Keep it in sync with `configure.ac` `AC_INIT`.

Older mixer-only freeze: `v0.1.16-ttns-pre.7` on `ttns-mixer`.

Upstream BUTT is `0.1.16`; this fork adds the TTNS mixer UI and is versioned separately.

---

## Download (what to send DJs)

GitHub Releases: [dom-robinson/ttns-butt/releases](https://github.com/dom-robinson/ttns-butt/releases)

| Who | File | What they do |
|-----|------|----------------|
| **Mac (Apple Silicon, current macOS)** | `TTNS-Deck-…-macos-arm64.dmg` | Open the disk image, drag **TTNS Deck** to Applications, open it |
| **Mac (Intel)** | `TTNS-Deck-…-macos-x64.dmg` | Same |
| **Mac (Monterey 12, Apple Silicon)** | `TTNS-Deck-…-macos-arm64-monterey12.dmg` | Same; current-OS DMGs will not launch on 12.x |
| **Windows 10+** | `TTNS-Deck-…-windows-x64-setup.exe` | Double-click the installer (no admin). Shortcuts in Start Menu |
| **Linux x86_64** | `ttns-deck-linux-x86_64.tar.gz` | `tar xzf … && ./ttns-deck-linux-*/run-ttns-deck.sh` |

**TTNS Remote** is inside the same Mac DMG and the Windows installer.

Send the **.dmg / .exe file**. Do **not** send a raw `.app` via Dropbox or email — macOS apps are folders, and recipients only see `Contents`.

Portable zips (`ttns-deck-*-macos.zip`, `ttns-deck-win64.zip`) are still attached for ops.

Each package includes `legal/` (or `Resources/legal/` on macOS) with `COPYING`, `DISTRIBUTION_LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, and the Fraunhofer FDK-AAC license.

See [`USER_GUIDE.md`](USER_GUIDE.md) (full) or [`TTNS_DJ_GUIDE.md`](TTNS_DJ_GUIDE.md) (short).

For a Dropbox folder of every OS, run `./scripts/package-dj-testers.sh /path/to/downloaded-ci-artifacts`.

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
./scripts/generate-icons.sh
```

Commit updated `assets/ttns-deck.icns` and `assets/ttns-deck.ico`.

---

## Local packaging

```bash
autoreconf -fi && ./configure && make -C src
./scripts/build-release.sh
```

| OS | Output |
|----|--------|
| macOS | `dist/macos/TTNS Deck.app`, `.zip`, `TTNS-Deck-<ver>-macos-<arch>.dmg` |
| macOS 12 arm64 | `./scripts/build-macos12-app.sh` |
| Linux | `dist/linux/ttns-deck-linux-$(uname -m).tar.gz` |
| Windows (MSYS2) | `dist/windows/ttns-deck-win64.zip`; with Inno Setup, `TTNS-Deck-<ver>-windows-x64-setup.exe` |

---

## CI (GitHub Actions)

Workflow: [`.github/workflows/build.yml`](../.github/workflows/build.yml)

| Job | Runner | Notes |
|-----|--------|-------|
| `build-macos` | `macos-latest` (arm64), `macos-15-intel` (x64) | Homebrew deps; `.app` + zip + **dmg**. Current macOS only (Homebrew bottles). |
| `build-linux` | `ubuntu-latest` | apt; `build-linux.sh` |
| `build-windows` | `windows-latest` + MSYS2 | DLL zip + **Inno Setup** `setup.exe` |

Pushes to `master` / `ttns-mixer` / `feature/remote-dial-in` and tags `v*` trigger builds. Tags create a **pre-release** with the packages above.

---

## macOS Gatekeeper (ops)

CI builds are **unsigned**. DJs: **Done** (not Move to Bin) → Privacy & Security → **Open Anyway**. Details: [`MACOS_GATEKEEPER.md`](MACOS_GATEKEEPER.md).

For distribution outside the team:

```bash
codesign --force --deep --sign "Developer ID Application: …" "dist/macos/TTNS Deck.app"
```

---

## Known gaps

- Default GitHub macOS builds need a **current** macOS (the runner’s Homebrew libs). Monterey testers need the **monterey12** DMG from `build-macos12-app.sh`.
- Linux **arm64** tarball: build on arm64 hardware (`build-linux.sh` names the arch).
- Intel macOS CI uses `macos-15-intel` (GitHub’s last x86_64 macOS image; retires ~2027).
