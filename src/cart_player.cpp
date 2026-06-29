#include "cart_player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cart_loader.h"
#include "ttns_audio.h"

typedef struct
{
    short *pcm;
    int total_frames;
    int channels;
    char label[32];
    int mode;

    volatile int fire;
    int playing;
    int latched;
    int position;
    float fade;
    int fade_samples;
    int fading_out;
} cart_slot_t;

static cart_slot_t slots[TTNS_CART_SLOTS];
static int stream_sr = 44100;

static int fade_samples_for_sr(int sr)
{
    return (sr * TTNS_CART_FADE_MS) / 1000;
}

static void cart_stop_slot(cart_slot_t *s)
{
    s->playing = 0;
    s->latched = 0;
    s->position = 0;
    s->fade = 0.0f;
    s->fading_out = 0;
}

static void cart_start_slot(cart_slot_t *s)
{
    if (!s->pcm || s->total_frames <= 0)
        return;

    s->playing = 1;
    s->latched = (s->mode == TTNS_CART_LOOP) ? 1 : 0;
    s->position = 0;
    s->fade = 0.0f;
    s->fading_out = 0;
}

void ttns_cart_init(int samplerate)
{
    int i;
    stream_sr = samplerate;
    memset(slots, 0, sizeof(slots));
    for (i = 0; i < TTNS_CART_SLOTS; i++)
        slots[i].fade_samples = fade_samples_for_sr(stream_sr);
}

void ttns_cart_shutdown(void)
{
    int i;
    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        free(slots[i].pcm);
        slots[i].pcm = NULL;
        slots[i].total_frames = 0;
        cart_stop_slot(&slots[i]);
    }
}

int ttns_cart_load(int slot, const char *path)
{
    cart_slot_t *s;
    int frames = 0;
    short *pcm;

    if (slot < 0 || slot >= TTNS_CART_SLOTS || !path || !path[0])
        return -1;

    s = &slots[slot];
    cart_stop_slot(s);
    free(s->pcm);
    s->pcm = NULL;
    s->total_frames = 0;

    pcm = cart_load_stereo_pcm(path, stream_sr, &frames);
    if (!pcm || frames <= 0)
        return -1;

    s->pcm = pcm;
    s->total_frames = frames;
    s->channels = 2;
    s->fade_samples = fade_samples_for_sr(stream_sr);
    return 0;
}

void ttns_cart_clear(int slot)
{
    cart_slot_t *s;

    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return;

    s = &slots[slot];
    cart_stop_slot(s);
    free(s->pcm);
    s->pcm = NULL;
    s->total_frames = 0;
    s->label[0] = '\0';
}

void ttns_cart_set_mode(int slot, int mode)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return;
    slots[slot].mode = mode;
}

void ttns_cart_set_label(int slot, const char *label)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return;
    if (!label)
        slots[slot].label[0] = '\0';
    else
        strncpy(slots[slot].label, label, sizeof(slots[slot].label) - 1);
    slots[slot].label[sizeof(slots[slot].label) - 1] = '\0';
}

const char *ttns_cart_get_label(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return "";
    if (slots[slot].label[0])
        return slots[slot].label;
    return NULL;
}

void ttns_cart_trigger(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return;
    slots[slot].fire = 1;
}

void ttns_cart_render(short *out_stereo, int frames)
{
    int i, f;
    cart_slot_t *s;

    for (f = 0; f < frames; f++)
        out_stereo[f * 2] = out_stereo[f * 2 + 1] = 0;

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        s = &slots[i];

        if (s->fire)
        {
            s->fire = 0;
            if (s->playing && s->mode == TTNS_CART_LOOP && s->latched)
            {
                s->fading_out = 1;
                s->latched = 0;
            }
            else if (!s->playing)
            {
                cart_start_slot(s);
            }
            else if (s->mode == TTNS_CART_ONESHOT)
            {
                cart_start_slot(s);
            }
        }

        if (!s->playing || !s->pcm)
            continue;

        for (f = 0; f < frames; f++)
        {
            int l, r;
            float g;

            if (!s->playing)
                break;

            if (s->position >= s->total_frames)
            {
                if (s->mode == TTNS_CART_LOOP && s->latched)
                    s->position = 0;
                else
                {
                    cart_stop_slot(s);
                    break;
                }
            }

            l = s->pcm[s->position * 2];
            r = s->pcm[s->position * 2 + 1];

            if (!s->fading_out)
            {
                if (s->fade < 1.0f)
                {
                    s->fade += 1.0f / (float)(s->fade_samples > 0 ? s->fade_samples : 1);
                    if (s->fade > 1.0f)
                        s->fade = 1.0f;
                }
            }
            else
            {
                s->fade -= 1.0f / (float)(s->fade_samples > 0 ? s->fade_samples : 1);
                if (s->fade <= 0.0f)
                {
                    cart_stop_slot(s);
                    break;
                }
            }

            if (s->mode == TTNS_CART_ONESHOT && !s->fading_out &&
                s->position >= s->total_frames - s->fade_samples)
            {
                s->fading_out = 1;
            }

            g = s->fade;
            out_stereo[f * 2] = ttns_clamp16(out_stereo[f * 2] + (int)(l * g));
            out_stereo[f * 2 + 1] = ttns_clamp16(out_stereo[f * 2 + 1] + (int)(r * g));
            s->position++;
        }
    }
}

int ttns_cart_any_playing(void)
{
    int i;
    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        if (slots[i].playing)
            return 1;
    }
    return 0;
}

int ttns_cart_is_playing(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return 0;
    return slots[slot].playing;
}

int ttns_cart_has_audio(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return 0;
    return (slots[slot].pcm != NULL && slots[slot].total_frames > 0);
}

int ttns_cart_get_mode(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return TTNS_CART_ONESHOT;
    return slots[slot].mode;
}
