#ifndef TTNS_REMOTE_H
#define TTNS_REMOTE_H

#include <stddef.h>

#ifndef TTNS_REMOTE_SLOTS
#define TTNS_REMOTE_SLOTS 4
#endif
#define TTNS_REMOTE_NAME_LEN 32
#ifndef TTNS_REMOTE_ROOM_LEN
#define TTNS_REMOTE_ROOM_LEN 12
#endif

enum {
    TTNS_REMOTE_IDLE = 0,
    TTNS_REMOTE_WAITING = 1,
    TTNS_REMOTE_CONNECTED = 2,
    TTNS_REMOTE_ERROR = 3
};

/* Init/shutdown tied to PortAudio mix lifetime (samplerate + callback frames). */
int ttns_remote_init(int samplerate, int frames);
void ttns_remote_shutdown(void);
int ttns_remote_is_inited(void);

/* Decoded uplink from a remote peer (network/session thread). Mono or stereo. */
int ttns_remote_push_uplink(int slot, const short *pcm, int frames, int channels);

/* Audio callback: fill per-slot stereo work buffers from uplink rbs. */
void ttns_remote_prepare_block(int frames);

/* After prepare_block: stereo interleaved PCM for this callback, or NULL if idle. */
const short *ttns_remote_uplink_stereo(int slot);

int ttns_remote_uplink_peak(int slot);
int ttns_remote_duck_peak(void);

float ttns_remote_effective_gain(int slot);

/* Mix-minus return path (encoder/session thread reads). Host writes at mix SR;
 * ring stores Opus-rate (48 kHz) stereo after optional SRC. */
void ttns_remote_write_mix_minus(int slot, const short *stereo, int frames);
/* Frames currently available at Opus rate (stereo). */
int ttns_remote_mix_minus_avail(int slot);
/* Read exactly `frames` Opus-rate stereo frames, or 0 without consuming. */
int ttns_remote_read_mix_minus(int slot, short *stereo, int frames);

void ttns_remote_set_state(int slot, int state);
int ttns_remote_state(int slot);
int ttns_remote_is_live(int slot);

void ttns_remote_set_name(int slot, const char *name);
const char *ttns_remote_name(int slot);

/* Operator kick / clear slot. */
void ttns_remote_clear_slot(int slot);

/* Dev aid: 440 Hz tone into uplink when slot is "test" connected. */
void ttns_remote_set_test_tone(int slot, int enable);
int ttns_remote_test_tone(int slot);

/* Host room code helpers (signaling comes later). */
void ttns_remote_generate_room_code(char *out, size_t out_len);
const char *ttns_remote_room_code(void);
void ttns_remote_set_room_code(const char *code);
void ttns_remote_normalize_room(char *dst, size_t dst_len, const char *src);

#endif
