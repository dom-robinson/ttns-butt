# TTNS WAN dial-in (WRX)

LAN Opus/TCP remains the preferred path. When the Remote cannot discover the Deck
on the LAN, it falls back to the **WRX WebSocket relay**:

`wss://wrx.liveencode.com/ttns/ws`

This is a **signaling + media relay** (always through WRX when on WAN). Coturn STUN/TURN
is already on WRX for a future true WebRTC P2P step; iceServers are advertised by `/ice`
and `host_ok` / `join_ok`.

## Topology

```text
Deck (anywhere)  --WSS-->  Traefik :443  --> ttns-signal :8095  <--WSS--  Remote
                              |
                           room code match
                              |
                         relay Opus packets
```

## Deploy on WRX

```bash
cd tools/wrx_admin
./deploy-ttns-signal-to-wrx.sh 192.168.0.86
```

Requires:

- Router TCP **443** → WRX (already)
- TURN ports already forwarded (for future P2P / other apps)

Optional later (Phase8 DNS): add `remote.liveencode.com` → same public IP as
`wrx.liveencode.com`. Traefik already has a Host rule for it; apps can then use
`TTNS_WAN_URL=wss://remote.liveencode.com/ws`.

## Verify

```bash
curl -sS https://wrx.liveencode.com/ttns/health
# {"ok":true,"service":"ttns-signal","rooms":0,"remotes":0}
```

## App usage

1. Deck: Accept on (publishes LAN + registers room on WRX)
2. Remote: enter room code → Connect
   - Same LAN → LAN path
   - Else → internet via WRX

Override signal URL: `export TTNS_WAN_URL=wss://wrx.liveencode.com/ttns/ws`
