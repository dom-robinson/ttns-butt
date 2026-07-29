#include "ttns_audio.h"

#include <stdlib.h>
#include <math.h>

#include "cfg.h"

static float duck_smoothed = 1.0f;
static int duck_active = 0;

static volatile int meter_line_peak = 0;
static volatile int meter_mic_peak = 0;
static volatile int meter_cart_peak = 0;

int ttns_mic_effective_mute(void)
{
    return cfg.ttns.mic_mute ? 1 : 0;
}

void ttns_meters_push(int line_peak, int mic_peak, int cart_peak)
{
    meter_line_peak = line_peak;
    meter_mic_peak = mic_peak;
    meter_cart_peak = cart_peak;
}

void ttns_meters_reset(void)
{
    meter_line_peak = 0;
    meter_mic_peak = 0;
    meter_cart_peak = 0;
}

void ttns_meters_reset_mic(void)
{
    meter_mic_peak = 0;
}

void ttns_meters_poll(int *line_peak, int *mic_peak, int *cart_peak)
{
    if (line_peak)
        *line_peak = meter_line_peak;
    if (mic_peak)
        *mic_peak = meter_mic_peak;
    if (cart_peak)
        *cart_peak = meter_cart_peak;
}

int ttns_clamp16(int sample)
{
    if (sample > 32767)
        return 32767;
    if (sample < -32768)
        return -32768;
    return sample;
}

void ttns_peak_lr(const short *buf, int frames, int channels, int *lpeak, int *rpeak)
{
    int i;
    int lp = 0;
    int rp = 0;

    for (i = 0; i < frames; i++)
    {
        int l = abs(buf[i * channels]);
        int r = (channels > 1) ? abs(buf[i * channels + 1]) : l;
        if (l > lp) lp = l;
        if (r > rp) rp = r;
    }
    *lpeak = lp;
    *rpeak = rp;
}

int ttns_mic_peak(const short *mic, int mic_channels, int frames)
{
    int lp, rp;
    ttns_peak_lr(mic, frames, mic_channels, &lp, &rp);
    return (lp > rp) ? lp : rp;
}

void ttns_mixer_reset(void)
{
    duck_smoothed = 1.0f;
    duck_active = 0;
}

float ttns_duck_gain_update(int mic_peak, int samplerate, int frames,
                            float threshold_lin, float depth_lin,
                            int attack_ms, int release_ms)
{
    float target;
    float coeff;
    int threshold = (int)(threshold_lin * 32767.0f);

    if (threshold < 1)
        threshold = 1;
    if (depth_lin < 0.0f)
        depth_lin = 0.0f;
    if (depth_lin > 1.0f)
        depth_lin = 1.0f;

    target = (mic_peak >= threshold) ? depth_lin : 1.0f;
    duck_active = (target < 0.99f);

    if (attack_ms < 1)
        attack_ms = 1;
    if (release_ms < 1)
        release_ms = 1;

    if (target < duck_smoothed)
        coeff = 1.0f - expf(-(float)frames / ((float)samplerate * (float)attack_ms / 1000.0f));
    else
        coeff = 1.0f - expf(-(float)frames / ((float)samplerate * (float)release_ms / 1000.0f));

    duck_smoothed += (target - duck_smoothed) * coeff;
    if (duck_smoothed < depth_lin)
        duck_smoothed = depth_lin;
    if (duck_smoothed > 1.0f)
        duck_smoothed = 1.0f;

    return duck_smoothed;
}

int ttns_ducking_active(void)
{
    return duck_active;
}

void ttns_process_mix(short *out, const short *mic, int mic_channels,
                      const short *line, const short *cart, int frames,
                      float mic_gain, float line_gain, float cart_gain,
                      float duck_gain)
{
    ttns_process_mix_ex(out, mic, mic_channels, line, cart, frames,
                        mic_gain, line_gain, cart_gain, duck_gain,
                        NULL, NULL, -1);
}

void ttns_process_mix_ex(short *out, const short *mic, int mic_channels,
                         const short *line, const short *cart, int frames,
                         float mic_gain, float line_gain, float cart_gain,
                         float duck_gain,
                         const short *remote_stereo[TTNS_REMOTE_SLOTS],
                         const float remote_gain[TTNS_REMOTE_SLOTS],
                         int exclude_remote)
{
    int i;
    int r;

    for (i = 0; i < frames; i++)
    {
        int mic_l, mic_r;
        int line_l = line[i * 2];
        int line_r = line[i * 2 + 1];
        int cart_l = 0;
        int cart_r = 0;
        float bus_l, bus_r;
        float voice_l = 0.0f;
        float voice_r = 0.0f;

        if (cart)
        {
            cart_l = cart[i * 2];
            cart_r = cart[i * 2 + 1];
        }

        if (mic && mic_gain != 0.0f)
        {
            if (mic_channels >= 2)
            {
                mic_l = mic[i * 2];
                mic_r = mic[i * 2 + 1];
            }
            else
            {
                mic_l = mic_r = mic[i];
            }
            voice_l += mic_l * mic_gain;
            voice_r += mic_r * mic_gain;
        }

        if (remote_stereo && remote_gain)
        {
            for (r = 0; r < TTNS_REMOTE_SLOTS; r++)
            {
                const short *rs;
                float g;

                if (r == exclude_remote)
                    continue;
                rs = remote_stereo[r];
                g = remote_gain[r];
                if (!rs || g == 0.0f)
                    continue;
                voice_l += rs[i * 2] * g;
                voice_r += rs[i * 2 + 1] * g;
            }
        }

        bus_l = line_l * line_gain * duck_gain + cart_l * cart_gain * duck_gain;
        bus_r = line_r * line_gain * duck_gain + cart_r * cart_gain * duck_gain;

        out[i * 2] = ttns_clamp16((int)(bus_l + voice_l));
        out[i * 2 + 1] = ttns_clamp16((int)(bus_r + voice_r));
    }
}
