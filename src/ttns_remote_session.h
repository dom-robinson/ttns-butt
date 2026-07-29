#ifndef TTNS_REMOTE_SESSION_H
#define TTNS_REMOTE_SESSION_H

/*
 * Signaling + WebRTC transport for remote dial-in.
 * Phase 1: host room lifecycle stubs only (no network yet).
 * Phase 2: WebRTC/Opus via libdatachannel (or similar) + TURN.
 */

int ttns_remote_session_init(void);
void ttns_remote_session_shutdown(void);

/* Deck (host): publish room and accept up to TTNS_REMOTE_SLOTS peers. */
int ttns_remote_session_host_start(void);
void ttns_remote_session_host_stop(void);
int ttns_remote_session_host_running(void);
/* Re-advertise after New code while Accept is already on. */
void ttns_remote_session_host_refresh_discovery(void);

/* Remote client: join a published room code. */
int ttns_remote_session_client_join(const char *room_code);
void ttns_remote_session_client_leave(void);
int ttns_remote_session_client_connected(void);

const char *ttns_remote_session_status_text(void);

#endif
