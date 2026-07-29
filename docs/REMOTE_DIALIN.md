# Remote dial-in co-host (TTNS Deck + TTNS Remote)

Work lives on branch **`feature/remote-dial-in`**. The current DJ release is preserved.

## How to get back to the current release

The pre.7 release is frozen in three places (any one is enough):

| Ref | What |
|-----|------|
| Tag | `v0.1.16-ttns-pre.7` |
| Branch | `release/v0.1.16-ttns-pre.7` |
| Branch | `ttns-mixer` (still at pre.7 until remote work is merged) |

```bash
# Run / build the known-good release
git checkout v0.1.16-ttns-pre.7
# or
git checkout release/v0.1.16-ttns-pre.7

./scripts/build-release.sh
```

Do **not** force-push `ttns-mixer` or move/delete `v0.1.16-ttns-pre.7`. Merge remote work via PR when ready.

## Product goals

- Up to **4** remote co-hosts
- Easy join: room code, no inbound firewall holes (WebRTC + TURN)
- Each remote is a mixable channel on Deck (fader / mute / meter) and triggers ducking
- Each remote hears **mix-minus** (program minus themselves; includes host + other remotes)
- Separate **TTNS Remote** app: devices, connect, levels, mute / PTT

## Transport (planned)

WebRTC + Opus, STUN/TURN (e.g. WRX coturn). Signaling: short room code. Icecast stays one-way broadcast only.

## Implementation status

| Phase | Status |
|-------|--------|
| **0** WebRTC duplex spike + TURN proof | Not started |
| **1** 4-slot remote bus + mix-minus + operator UI + test tone | **In progress on this branch** |
| **2** Signaling + WebRTC sessions | Stub only (`ttns_remote_session.*`) |
| **3** Remote app audio I/O + polish + packaging | UI shell only |
| **4** Multi-peer hardening, LAN mDNS, etc. | Later |

### What you can try now (Deck)

1. Build `feature/remote-dial-in`
2. Open TTNS Deck — new **Remote** section under carts (Accept, room code, R1–R4)
3. Click **T** on a remote row to inject a 440 Hz **test tone** into that slot (no network)
4. Confirm the remote fader/meter, mute, and ducking behave like a second mic

### Binaries

- `butt` → installed as `ttns-deck`
- `ttns_remote` → installed as `ttns-remote` (UI shell; Connect shows “not implemented yet”)

## Key files

| Area | Files |
|------|--------|
| Remote bus / mix-minus rbs | `src/ttns_remote.cpp` |
| Mix math | `src/ttns_audio.cpp` (`ttns_process_mix_ex`) |
| Callback integration | `src/port_audio.cpp` |
| Operator UI | `src/ttns_ui.cpp` |
| Session stub | `src/ttns_remote_session.cpp` |
| Remote app | `src/ttns_remote_app.cpp` |
