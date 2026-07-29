#ifndef TTNS_REMOTE_NET_H
#define TTNS_REMOTE_NET_H

#include <stddef.h>
#include "ttns_remote_proto.h"

/* Shared low-level helpers used by Deck host and TTNS Remote client. */

int ttns_rnet_send_packet(int fd, uint8_t type, uint8_t slot, uint16_t seq,
                          const void *payload, uint16_t len);
/* Build packet into out; returns total bytes or -1. */
int ttns_rnet_build_packet(void *out, size_t out_cap, uint8_t type, uint8_t slot,
                           uint16_t seq, const void *payload, uint16_t len);
/* Returns payload length (>=0), 0 on timeout/empty, -1 on disconnect/error. */
int ttns_rnet_recv_packet(int fd, ttns_remote_hdr_t *hdr, void *payload,
                          uint16_t max_len, int timeout_ms);

int ttns_rnet_listen(int port);
int ttns_rnet_accept(int listen_fd, char *peer_ip, int peer_ip_len, int timeout_ms);
int ttns_rnet_connect(const char *host, int port, int timeout_ms);
void ttns_rnet_close(int *fd);

/* UDP room discovery on the LAN. */
int ttns_rnet_discovery_reply_start(const char *room_code, int tcp_port);
void ttns_rnet_discovery_reply_stop(void);
/* Blocks up to timeout_ms. On success fills host (IPv4 string) and *tcp_port. */
int ttns_rnet_discover_room(const char *room_code, char *host, int host_len,
                            int *tcp_port, int timeout_ms);

/* Uppercase/trim room codes so discover + HELLO match. */
void ttns_rnet_normalize_room(char *dst, size_t dst_len, const char *src);

#endif
