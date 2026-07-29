#ifndef TTNS_REMOTE_PROTO_H
#define TTNS_REMOTE_PROTO_H

#include <stdint.h>

#define TTNS_REMOTE_UDP_PORT      38741
#define TTNS_REMOTE_TCP_PORT_BASE 38750
#define TTNS_REMOTE_SAMPLERATE    48000
#define TTNS_REMOTE_FRAME_MS      20
#define TTNS_REMOTE_FRAME_SAMPLES (TTNS_REMOTE_SAMPLERATE * TTNS_REMOTE_FRAME_MS / 1000)
#define TTNS_REMOTE_OPUS_BR       64000
/* Mix-minus carries music/carts — higher bitrate than talkback uplink. */
#define TTNS_REMOTE_OPUS_BR_DOWN  96000
#define TTNS_REMOTE_MAX_PACKET    1500
#define TTNS_REMOTE_NAME_MAX      32

#define TTNS_REMOTE_MAGIC 0x53544E54u /* 'TTNS' little-endian host value via memcpy */

enum {
    TTNS_PKT_HELLO = 1,
    TTNS_PKT_HELLO_ACK = 2,
    TTNS_PKT_AUDIO = 3,
    TTNS_PKT_BYE = 4
};

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t type;
    uint8_t slot;
    uint16_t seq;
    uint16_t len;
} ttns_remote_hdr_t;
#pragma pack(pop)

#endif
