#ifndef TTNS_REMOTE_WAN_H
#define TTNS_REMOTE_WAN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default public signaling endpoint (Traefik → ttns-signal on WRX).
 * Uses wrx.liveencode.com/ttns until remote.liveencode.com DNS exists. */
#ifndef TTNS_WAN_SIGNAL_URL
#define TTNS_WAN_SIGNAL_URL "wss://wrx.liveencode.com/ttns/ws"
#endif

int ttns_wan_available(void);

/* Deck host: register room on WRX; remotes can join from the internet. */
int ttns_wan_host_start(const char *room_code, const char *display_name);
void ttns_wan_host_stop(void);
int ttns_wan_host_running(void);

/* Returns 1 and fills name if a WAN peer joined; 0 if none waiting. */
int ttns_wan_host_accept_peer(int *slot_out, char *name, size_t name_len);

/* Send/recv TTNS packets on the host multiplexed WebSocket. */
int ttns_wan_host_send(const void *packet, size_t len);
/* timeout_ms: 0 = nonblocking. Returns bytes, 0 timeout, -1 error/closed. */
int ttns_wan_host_recv(void *buf, size_t buflen, int timeout_ms);

/* Remote client join via WRX when LAN discovery fails. */
int ttns_wan_client_join(const char *room_code, const char *display_name,
                         int *slot_out);
void ttns_wan_client_leave(void);
int ttns_wan_client_connected(void);
int ttns_wan_client_send(const void *packet, size_t len);
int ttns_wan_client_recv(void *buf, size_t buflen, int timeout_ms);

const char *ttns_wan_last_error(void);

/* Probe https://core.liveencode.com — 1 reachable, 0 unreachable. */
int ttns_core_reach_get(void);
void ttns_core_reach_start(void);

#ifdef __cplusplus
}
#endif

#endif
