#include "ttns_remote.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cfg.h"
#include "ringbuffer.h"
#include "ttns_audio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    struct ringbuf up_rb;
    struct ringbuf down_rb;
    int up_inited;
    int down_inited;
    short *work;          /* stereo for current callback */
    volatile int peak;
    volatile int state;
    char name[TTNS_REMOTE_NAME_LEN];
    int test_tone;
    double tone_phase;
} ttns_remote_slot_t;

static ttns_remote_slot_t slots[TTNS_REMOTE_SLOTS];
static int remote_inited = 0;
static int remote_samplerate = 48000;
static int remote_frames = 0;
static char room_code[TTNS_REMOTE_ROOM_LEN];

static unsigned int ttns_remote_rb_bytes(int frames)
{
    /* ~0.5s of stereo PCM headroom */
    int keep = frames * 2 * 25;
    if (keep < 8192)
        keep = 8192;
    return (unsigned int)keep * sizeof(short);
}

static void ttns_remote_slot_free(ttns_remote_slot_t *s)
{
    if (s->up_inited)
    {
        rb_free(&s->up_rb);
        s->up_inited = 0;
    }
    if (s->down_inited)
    {
        rb_free(&s->down_rb);
        s->down_inited = 0;
    }
    free(s->work);
    s->work = NULL;
    s->peak = 0;
    s->state = TTNS_REMOTE_IDLE;
    s->test_tone = 0;
    s->tone_phase = 0.0;
    s->name[0] = '\0';
}

void ttns_remote_shutdown(void)
{
    int i;

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
        ttns_remote_slot_free(&slots[i]);

    remote_inited = 0;
    remote_frames = 0;
}

int ttns_remote_init(int samplerate, int frames)
{
    int i;
    unsigned int bytes;

    ttns_remote_shutdown();

    if (samplerate < 8000 || frames < 1)
        return 1;

    remote_samplerate = samplerate;
    remote_frames = frames;
    bytes = ttns_remote_rb_bytes(frames);

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        ttns_remote_slot_t *s = &slots[i];

        memset(s, 0, sizeof(*s));
        s->work = (short *)malloc((size_t)frames * 2 * sizeof(short));
        if (!s->work)
        {
            ttns_remote_shutdown();
            return 1;
        }
        if (rb_init(&s->up_rb, bytes) != 0)
        {
            ttns_remote_shutdown();
            return 1;
        }
        s->up_inited = 1;
        if (rb_init(&s->down_rb, bytes) != 0)
        {
            ttns_remote_shutdown();
            return 1;
        }
        s->down_inited = 1;
        snprintf(s->name, sizeof(s->name), "Remote %d", i + 1);
    }

    if (room_code[0] == '\0')
        ttns_remote_generate_room_code(room_code, sizeof(room_code));

    remote_inited = 1;
    return 0;
}

int ttns_remote_is_inited(void)
{
    return remote_inited;
}

static void ttns_remote_uplink_to_stereo(short *dest, const short *src,
                                        int frames, int channels, int got_frames)
{
    int i;

    for (i = 0; i < frames; i++)
    {
        if (i < got_frames)
        {
            if (channels >= 2)
            {
                dest[i * 2] = src[i * 2];
                dest[i * 2 + 1] = src[i * 2 + 1];
            }
            else
            {
                dest[i * 2] = dest[i * 2 + 1] = src[i];
            }
        }
        else
        {
            dest[i * 2] = 0;
            dest[i * 2 + 1] = 0;
        }
    }
}

static void ttns_remote_fill_test_tone(ttns_remote_slot_t *s, int frames)
{
    int i;
    double step = 2.0 * M_PI * 440.0 / (double)remote_samplerate;

    for (i = 0; i < frames; i++)
    {
        short v = (short)(sinf(s->tone_phase) * 8000.0);
        s->work[i * 2] = v;
        s->work[i * 2 + 1] = v;
        s->tone_phase += step;
        if (s->tone_phase > 2.0 * M_PI)
            s->tone_phase -= 2.0 * M_PI;
    }
}

int ttns_remote_push_uplink(int slot, const short *pcm, int frames, int channels)
{
    ttns_remote_slot_t *s;
    short stereo[4096 * 2];
    int n;
    int off;

    if (!remote_inited || slot < 0 || slot >= TTNS_REMOTE_SLOTS || !pcm || frames < 1)
        return 1;
    if (channels < 1)
        channels = 1;
    if (channels > 2)
        channels = 2;

    s = &slots[slot];
    if (!s->up_inited || s->state != TTNS_REMOTE_CONNECTED)
        return 1;

    /* Normalize to stereo in the ring buffer. */
    off = 0;
    while (off < frames)
    {
        n = frames - off;
        if (n > 4096)
            n = 4096;
        ttns_remote_uplink_to_stereo(stereo, pcm + off * channels, n, channels, n);
        rb_write_drop(&s->up_rb, (char *)stereo, (unsigned int)n * 2 * sizeof(short));
        off += n;
    }
    return 0;
}

void ttns_remote_prepare_block(int frames)
{
    int slot;
    int use_frames = frames;

    if (!remote_inited || frames < 1)
        return;
    if (use_frames > remote_frames)
        use_frames = remote_frames;

    for (slot = 0; slot < TTNS_REMOTE_SLOTS; slot++)
    {
        ttns_remote_slot_t *s = &slots[slot];
        int filled;
        int need;
        int got;
        int avail_frames;

        if (!s->work)
            continue;

        if (s->test_tone && s->state == TTNS_REMOTE_CONNECTED)
        {
            ttns_remote_fill_test_tone(s, use_frames);
            s->peak = ttns_mic_peak(s->work, 2, use_frames);
            continue;
        }

        if (s->state != TTNS_REMOTE_CONNECTED || !s->up_inited)
        {
            memset(s->work, 0, (size_t)use_frames * 2 * sizeof(short));
            s->peak = 0;
            continue;
        }

        need = use_frames * 2 * (int)sizeof(short);
        filled = rb_filled(&s->up_rb);
        got = 0;
        if (filled >= need)
        {
            rb_read_len(&s->up_rb, (char *)s->work, (unsigned int)need);
            got = use_frames;
        }
        else if (filled >= (int)(2 * sizeof(short)))
        {
            avail_frames = filled / (int)(2 * sizeof(short));
            rb_read_len(&s->up_rb, (char *)s->work,
                        (unsigned int)avail_frames * 2 * sizeof(short));
            got = avail_frames;
            memset(s->work + got * 2, 0, (size_t)(use_frames - got) * 2 * sizeof(short));
        }
        else
        {
            memset(s->work, 0, (size_t)use_frames * 2 * sizeof(short));
        }

        (void)got;
        s->peak = ttns_mic_peak(s->work, 2, use_frames);
    }
}

const short *ttns_remote_uplink_stereo(int slot)
{
    if (!remote_inited || slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return NULL;
    if (slots[slot].state != TTNS_REMOTE_CONNECTED)
        return NULL;
    return slots[slot].work;
}

int ttns_remote_uplink_peak(int slot)
{
    if (!remote_inited || slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return 0;
    return slots[slot].peak;
}

int ttns_remote_duck_peak(void)
{
    int i;
    int pk = 0;

    if (!remote_inited)
        return 0;

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        int p;

        if (ttns_remote_effective_gain(i) <= 0.0f)
            continue;
        p = slots[i].peak;
        if (p > pk)
            pk = p;
    }
    return pk;
}

float ttns_remote_effective_gain(int slot)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return 0.0f;
    if (!ttns_remote_is_live(slot))
        return 0.0f;
    if (cfg.ttns.remote_mute[slot])
        return 0.0f;
    return cfg.ttns.remote_gain[slot];
}

void ttns_remote_write_mix_minus(int slot, const short *stereo, int frames)
{
    size_t bytes;

    if (!remote_inited || slot < 0 || slot >= TTNS_REMOTE_SLOTS || !stereo || frames < 1)
        return;
    if (!slots[slot].down_inited || slots[slot].state != TTNS_REMOTE_CONNECTED)
        return;

    bytes = (size_t)frames * 2 * sizeof(short);
    rb_write_drop(&slots[slot].down_rb, (char *)stereo, (unsigned int)bytes);
}

int ttns_remote_read_mix_minus(int slot, short *stereo, int max_frames)
{
    int filled;
    int frames;
    unsigned int need;

    if (!remote_inited || slot < 0 || slot >= TTNS_REMOTE_SLOTS || !stereo || max_frames < 1)
        return 0;
    if (!slots[slot].down_inited)
        return 0;

    filled = rb_filled(&slots[slot].down_rb);
    frames = filled / (int)(2 * sizeof(short));
    if (frames > max_frames)
        frames = max_frames;
    if (frames < 1)
        return 0;

    need = (unsigned int)frames * 2 * sizeof(short);
    rb_read_len(&slots[slot].down_rb, (char *)stereo, need);
    return frames;
}

void ttns_remote_set_state(int slot, int state)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    slots[slot].state = state;
    if (state != TTNS_REMOTE_CONNECTED)
    {
        slots[slot].peak = 0;
        slots[slot].test_tone = 0;
    }
}

int ttns_remote_state(int slot)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return TTNS_REMOTE_IDLE;
    return slots[slot].state;
}

int ttns_remote_is_live(int slot)
{
    return ttns_remote_state(slot) == TTNS_REMOTE_CONNECTED;
}

void ttns_remote_set_name(int slot, const char *name)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    if (!name || !name[0])
    {
        snprintf(slots[slot].name, sizeof(slots[slot].name), "Remote %d", slot + 1);
        return;
    }
    snprintf(slots[slot].name, sizeof(slots[slot].name), "%s", name);
}

const char *ttns_remote_name(int slot)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return "";
    return slots[slot].name;
}

void ttns_remote_clear_slot(int slot)
{
    ttns_remote_slot_t *s;

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;

    s = &slots[slot];
    s->state = TTNS_REMOTE_IDLE;
    s->test_tone = 0;
    s->peak = 0;
    if (s->up_inited)
    {
        int filled = rb_filled(&s->up_rb);
        if (filled > 0)
            rb_discard(&s->up_rb, (unsigned int)filled);
    }
    if (s->down_inited)
    {
        int filled = rb_filled(&s->down_rb);
        if (filled > 0)
            rb_discard(&s->down_rb, (unsigned int)filled);
    }
    snprintf(s->name, sizeof(s->name), "Remote %d", slot + 1);
}

void ttns_remote_set_test_tone(int slot, int enable)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    slots[slot].test_tone = enable ? 1 : 0;
    if (enable)
        slots[slot].state = TTNS_REMOTE_CONNECTED;
}

int ttns_remote_test_tone(int slot)
{
    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return 0;
    return slots[slot].test_tone;
}

void ttns_remote_generate_room_code(char *out, size_t out_len)
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    size_t i;
    unsigned seed;

    if (!out || out_len < 7)
        return;

    seed = (unsigned)time(NULL) ^ (unsigned)(size_t)out;
    for (i = 0; i < 6 && i + 1 < out_len; i++)
    {
        seed = seed * 1103515245u + 12345u;
        out[i] = alphabet[(seed >> 16) % (sizeof(alphabet) - 1)];
    }
    out[i] = '\0';
}

const char *ttns_remote_room_code(void)
{
    if (cfg.ttns.remote_room[0])
        return cfg.ttns.remote_room;
    return room_code;
}

void ttns_remote_set_room_code(const char *code)
{
    if (!code)
        return;
    snprintf(room_code, sizeof(room_code), "%s", code);
    snprintf(cfg.ttns.remote_room, sizeof(cfg.ttns.remote_room), "%s", code);
}
