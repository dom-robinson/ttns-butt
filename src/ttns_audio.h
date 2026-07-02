#ifndef TTNS_AUDIO_H
#define TTNS_AUDIO_H

#include <stddef.h>

#define TTNS_CART_SLOTS 8

int ttns_clamp16(int sample);

void ttns_peak_lr(const short *buf, int frames, int channels, int *lpeak, int *rpeak);
int ttns_mic_peak(const short *mic, int mic_channels, int frames);

void ttns_meters_push(int line_peak, int mic_peak, int cart_peak);
void ttns_meters_poll(int *line_peak, int *mic_peak, int *cart_peak);
void ttns_meters_reset(void);
void ttns_meters_reset_mic(void);

int ttns_mic_effective_mute(void);

void ttns_mixer_reset(void);
float ttns_duck_gain_update(int mic_peak, int samplerate, int frames,
                            float threshold_lin, float depth_lin,
                            int attack_ms, int release_ms);
int ttns_ducking_active(void);

/* line+cart bus with ducking, then add mic. cart may be NULL. */
void ttns_process_mix(short *out, const short *mic, int mic_channels,
                      const short *line, const short *cart, int frames,
                      float mic_gain, float line_gain, float cart_gain,
                      float duck_gain);

#endif
