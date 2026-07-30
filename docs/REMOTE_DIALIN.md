# Remote dial-in co-host (TTNS Deck + TTNS Remote)

Work lives on branch **`feature/remote-dial-in`**. Current crew test version: **`0.1.16-ttns-remote-dev.2`**.

End-user walkthrough: [`USER_GUIDE.md`](./USER_GUIDE.md).

## How to get back to the current release

| Ref | What |
|-----|------|
| Tag | `v0.1.16-ttns-pre.7` |
| Branch | `release/v0.1.16-ttns-pre.7` |
| Branch | `ttns-mixer` (still at pre.7 until remote work is merged) |

```bash
git checkout v0.1.16-ttns-pre.7
./scripts/build-release.sh
```

## Product goals

- Up to **4** remote co-hosts
- Easy join via room code
- Mixable channels + ducking on Deck
- Mix-minus return to each remote
- Separate **TTNS Remote** app

## Transport (current)

**LAN Opus/TCP + UDP discovery** (same Wi‑Fi/LAN as the Deck):

1. Deck **Accept** → listens on TCP ~38750+ and answers UDP room queries on **38741**
2. Remote broadcasts for the room code, connects, Opus 48 kHz mono both ways
3. If LAN discovery fails → **WAN WebSocket relay** via WRX (`wss://wrx.liveencode.com/ttns/ws`)

See [REMOTE_WAN.md](./REMOTE_WAN.md) for deploy and internet testing.

## Mix-minus

Each remote hears a **personal mix-minus**:

- Deck line (music), carts, Deck mic
- **Other** remotes who are live
- **Not** their own voice

Latency is kept low for wired phones; Bluetooth (AirPods) uses a larger jitter cushion (~120–160 ms) and music-oriented Opus on the downlink so the bed stays continuous.

## How to test (LAN)

### Terminal A — Deck

```bash
open dist/macos/TTNS\ Deck.app
```

1. Settings → Monitor Output = headphones (to hear remotes)
2. Expand **Remotes** → tick **Accept** → note **Code** (e.g. `AGT3B8`)

### Terminal B — Remote (same machine or another on the LAN)

```bash
./dist/macos/ttns-remote
# or: ./src/ttns_remote
```

1. Pick mic + headphones
2. Enter the room code → **Connect**
3. Speak — you should appear on Deck as **R1** (etc.), hear mix-minus

### Offline mix check

Press **T** on a Deck remote row for a local 440 Hz test tone (no network).

## Key files

| Area | Files |
|------|--------|
| Mix / mix-minus | `ttns_remote.cpp`, `ttns_audio.cpp`, `port_audio.cpp` |
| UI | `ttns_ui.cpp` |
| Protocol / discovery | `ttns_remote_proto.h`, `ttns_remote_net.cpp` |
| Deck host session | `ttns_remote_session.cpp` |
| Remote app | `ttns_remote_app.cpp` |
| WAN relay | `ttns_remote_wan.cpp` |
