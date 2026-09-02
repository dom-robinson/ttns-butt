# macOS Gatekeeper — opening unsigned TTNS apps

Crew test builds are **not** Apple-notarized. After you open the **.dmg** (or unzip a portable zip), macOS often shows:

> “Apple could not verify … is free of malware”  
> with **Done** / **Move to Bin**

That is normal for unsigned DMGs. Do **not** Move to Bin.

## Fix (macOS Sequoia / recent macOS) — recommended

1. Click **Done** (leave the app where it is).
2. Open **System Settings → Privacy & Security**.
3. Scroll down to the security section. You should see a message that **TTNS Deck** or **TTNS Remote** was blocked.
4. Click **Open Anyway**, then confirm **Open**.

You only need this once per app (until you download a new copy).

## Fix — Terminal (fastest for ops)

After dragging the apps to Applications:

```bash
xattr -cr /Applications/TTNS\ Deck.app
xattr -cr /Applications/TTNS\ Remote.app
```

If you still have a portable zip instead of a DMG:

```bash
xattr -cr ~/Downloads/TTNS\ Deck.app
xattr -cr ~/Downloads/TTNS\ Remote.app
```

Then open the app again.

## Older tip (sometimes still works)

Finder → select the app → **right-click → Open** → **Open**.  
On newer macOS this often does **not** appear; use Privacy & Security or `xattr` instead.

## What to open

| Role | App |
|------|-----|
| DJ / host | **TTNS Deck.app** |
| Co-host | **TTNS Remote.app** |

Do **not** run the bare `ttns-remote` Unix binary from Terminal unless you are developing — co-hosts should use **TTNS Remote.app**.

## Still blocked?

- Confirm they opened the **.dmg** and dragged the app to Applications (do not send a raw `.app` via Dropbox).
- Confirm architecture: Apple Silicon → `macos-arm64`; Intel → `macos-x64`; Monterey 12 → `monterey12`.
- Ask them to send a screenshot of Privacy & Security after the block message appears.
