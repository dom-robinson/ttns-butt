# macOS Gatekeeper — opening unsigned TTNS apps

Crew test builds are **not** Apple-notarized. After download, macOS often shows:

> “Apple could not verify … is free of malware”  
> with **Done** / **Move to Bin**

That is normal for our zips. Do **not** Move to Bin.

## Fix (macOS Sequoia / recent macOS) — recommended

1. Click **Done** (leave the app where it is).
2. Open **System Settings → Privacy & Security**.
3. Scroll down to the security section. You should see a message that **TTNS Deck** or **TTNS Remote** was blocked.
4. Click **Open Anyway**, then confirm **Open**.

You only need this once per app (until you download a new copy).

## Fix — Terminal (fastest for ops)

In Terminal, point at the unzipped apps (adjust the path):

```bash
xattr -cr ~/Downloads/TTNS\ Deck.app
xattr -cr ~/Downloads/TTNS\ Remote.app
```

Or clear quarantine on the whole unzip folder:

```bash
xattr -cr ~/Downloads/TTNS-Deck-folder
```

Then double-click the app again.

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

- Confirm they unzipped fully (not running from inside the zip).
- Confirm architecture: Apple Silicon → `macos-arm64`; Intel → `macos-x64`.
- Ask them to send a screenshot of Privacy & Security after the block message appears.
