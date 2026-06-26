# TTNS data files

## `ttns-zones.json`

Icecast connection presets for TTNS deck zones. Loaded by the zone/slot picker (Phase D).

| Setting | Value |
|---------|--------|
| Host | `decks.thethursdaynightshow.com` |
| Port | `8080` |
| User | `source` |
| Type | Icecast |

**20 mounts:** four zones × five slots (`ttnszone{N}_{1-5}`), each with its own source password.

### Stream metadata (all zones)

| Field | Value |
|-------|--------|
| Description | `Live Now on TheThursdayNightShow and on TTNS.FM` |
| Genre | `eclectic` |

No other ICY fields are required for TTNS.

### Zone selection (v1)

DJs pick **Zone** (1–4) and **Slot** (1–5) manually. Scheduler auto-mapping is a future enhancement.

## Distribution

This repository stays **private**. Only compiled binaries are distributed to TTNS DJs. Source passwords live in this file for bundling into the app at build time.
