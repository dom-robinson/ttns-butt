// portaudio functions for butt
//
// Copyright 2007-2008 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <string.h>
#include <pthread.h>
#include <samplerate.h>

#ifdef _WIN32
 #include <windows.h>
#endif

#include "config.h"

#include "butt.h"
#include "cfg.h"
#include "port_audio.h"
#include "parseconfig.h"
#include "lame_encode.h"
#include "aac_encode.h"
#include "shoutcast.h"
#include "icecast.h"
#include "strfuncs.h"
#include "wav_header.h"
#include "ringbuffer.h"
#include "vu_meter.h"
#include "flgui.h"
#include "fl_funcs.h"
#include "util.h"
#include "ttns_audio.h"
#include "ttns_remote.h"
#include "cart_player.h"


int pa_frames = 2048;

char* encode_buf;
short *pa_pcm_buf;
int buf_index;
int buf_pos;
int framepacket_size;

bool try_to_connect;
bool pa_new_frames;
bool reconnect;

bool next_file;
FILE *next_fd;

struct ringbuf rec_rb;
struct ringbuf stream_rb;

SRC_STATE *srconv_state_stream = NULL;
SRC_STATE *srconv_state_record = NULL;
SRC_DATA srconv_stream;
SRC_DATA srconv_record;
float *srconv_stream_in_buf = NULL;
float *srconv_stream_out_buf = NULL;
float *srconv_record_in_buf = NULL;
float *srconv_record_out_buf = NULL;

pthread_t rec_thread;
pthread_t stream_thread;
pthread_mutex_t stream_mut, rec_mut;
pthread_cond_t  stream_cond, rec_cond;

PaStream *stream;
static PaStream *mic_stream = NULL;
static PaStream *monitor_stream = NULL;
static short *mic_pcm_buf = NULL;
static short *ttns_mix_buf = NULL;
static short *ttns_cart_buf = NULL;
static short *ttns_line_buf = NULL;
static short *ttns_mic_work_buf = NULL;
static short *monitor_mix_buf = NULL;
static short *ttns_mix_minus_buf = NULL;
static short *snd_conv_work_buf = NULL;
static int line_input_channels = 2;
static int mic_input_channels = 1;
static int ttns_use_dual_mic = 0;
static int ttns_use_shared_input = 0;
static volatile int snd_audio_active = 0;
static volatile int mic_capture_peak = 0;
static struct ringbuf ttns_mic_rb;
static struct ringbuf monitor_rb;
static int ttns_mic_rb_inited = 0;
static int monitor_rb_inited = 0;

static void ttns_mic_rb_shutdown(void)
{
    if (ttns_mic_rb_inited)
    {
        rb_free(&ttns_mic_rb);
        ttns_mic_rb_inited = 0;
    }
}

static void ttns_monitor_rb_shutdown(void)
{
    if (monitor_rb_inited)
    {
        rb_free(&monitor_rb);
        monitor_rb_inited = 0;
    }
}

static int ttns_mic_rb_start(void)
{
    size_t bytes = (size_t)pa_frames * (size_t)mic_input_channels * sizeof(short) * 64;

    ttns_mic_rb_shutdown();
    if (rb_init(&ttns_mic_rb, (unsigned int)bytes) != 0)
        return 1;
    ttns_mic_rb_inited = 1;
    return 0;
}

static int ttns_monitor_rb_start(void)
{
    size_t bytes = (size_t)pa_frames * 2 * sizeof(short) * 64;

    ttns_monitor_rb_shutdown();
    if (rb_init(&monitor_rb, (unsigned int)bytes) != 0)
        return 1;
    monitor_rb_inited = 1;
    return 0;
}

static int ttns_mic_rb_read(short *dest, int frames, int channels)
{
    unsigned int need;
    unsigned int target;
    int filled;

    if (!ttns_mic_rb_inited || !dest || frames <= 0 || channels < 1)
        return 0;

    need = (unsigned int)frames * (unsigned int)channels * sizeof(short);
    filled = rb_filled(&ttns_mic_rb);
    target = need * 4;

    if (filled > (int)(target * 2))
        rb_discard(&ttns_mic_rb, (unsigned int)filled - target);

    filled = rb_filled(&ttns_mic_rb);
    if (filled < (int)need)
    {
        memset(dest, 0, need);
        if (filled > 0)
            rb_read_len(&ttns_mic_rb, (char*)dest, (unsigned int)filled);
        return 0;
    }

    rb_read_len(&ttns_mic_rb, (char*)dest, need);
    return 1;
}

static int monitor_out_cb(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    unsigned int need;
    int filled;

    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (!outputBuffer || framesPerBuffer == 0)
        return paContinue;

    need = (unsigned int)framesPerBuffer * 2 * sizeof(short);

    if (!monitor_rb_inited)
    {
        memset(outputBuffer, 0, need);
        return paContinue;
    }

    filled = rb_filled(&monitor_rb);
    if (filled < (int)need)
    {
        memset(outputBuffer, 0, need);
        if (filled > 0)
            rb_read_len(&monitor_rb, (char*)outputBuffer, (unsigned int)filled);
        return paContinue;
    }

    rb_read_len(&monitor_rb, (char*)outputBuffer, need);
    return paContinue;
}

static void ttns_gather_remote_voices(const short *remote_stereo[TTNS_REMOTE_SLOTS],
                                      float remote_gain[TTNS_REMOTE_SLOTS])
{
    int r;

    for (r = 0; r < TTNS_REMOTE_SLOTS; r++)
    {
        remote_stereo[r] = ttns_remote_uplink_stereo(r);
        remote_gain[r] = ttns_remote_effective_gain(r);
    }
}

static void ttns_push_monitor_mix(int frameCount, const short *line_stereo,
                                  const short *mic_in, int mic_channels,
                                  float line_gain, float cart_gain,
                                  float duck_gain, float mic_g,
                                  const short *remote_stereo[TTNS_REMOTE_SLOTS],
                                  const float remote_gain[TTNS_REMOTE_SLOTS])
{
    size_t mon_bytes;
    float mon_mic_g;
    const short *mic_src = mic_in;
    int mic_ch = mic_channels;

    if (!monitor_rb_inited || !monitor_mix_buf || frameCount <= 0 || !line_stereo)
        return;

    mon_mic_g = cfg.ttns.mic_monitor_mute ? 0.0f : mic_g;
    if (!mic_src)
    {
        mic_src = line_stereo;
        mic_ch = 1;
        mon_mic_g = 0.0f;
    }

    ttns_process_mix_ex(monitor_mix_buf, mic_src, mic_ch, line_stereo, ttns_cart_buf,
                        frameCount, mon_mic_g, line_gain, cart_gain, duck_gain,
                        remote_stereo, remote_gain, -1);

    mon_bytes = (size_t)frameCount * 2 * sizeof(short);
    rb_write_drop(&monitor_rb, (char*)monitor_mix_buf, (unsigned int)mon_bytes);
}

static void ttns_free_mix_buffers(void)
{
    ttns_remote_shutdown();
    free(ttns_mix_buf);
    free(ttns_cart_buf);
    free(ttns_line_buf);
    free(ttns_mic_work_buf);
    free(monitor_mix_buf);
    free(ttns_mix_minus_buf);
    ttns_mix_buf = NULL;
    ttns_cart_buf = NULL;
    ttns_line_buf = NULL;
    ttns_mic_work_buf = NULL;
    monitor_mix_buf = NULL;
    ttns_mix_minus_buf = NULL;
}

static int ttns_alloc_mix_buffers(void)
{
    size_t line_samples = (size_t)pa_frames * 2;
    size_t mic_samples = (size_t)pa_frames * 2;

    /* snd_stop_streams() should have cleared these; guard stale pointers if not. */
    if (ttns_mix_buf || ttns_cart_buf || ttns_line_buf
        || ttns_mic_work_buf || monitor_mix_buf || ttns_mix_minus_buf)
        ttns_free_mix_buffers();

    ttns_cart_buf = (short*)malloc(line_samples * sizeof(short));
    ttns_line_buf = (short*)malloc(line_samples * sizeof(short));
    ttns_mix_buf = (short*)malloc(line_samples * sizeof(short));
    ttns_mic_work_buf = (short*)malloc(mic_samples * sizeof(short));
    monitor_mix_buf = (short*)malloc(line_samples * sizeof(short));
    ttns_mix_minus_buf = (short*)malloc(line_samples * sizeof(short));

    if (!ttns_cart_buf || !ttns_line_buf || !ttns_mix_buf || !ttns_mic_work_buf
        || !monitor_mix_buf || !ttns_mix_minus_buf)
    {
        ttns_free_mix_buffers();
        return 1;
    }

    if (ttns_remote_init(cfg.audio.samplerate, pa_frames) != 0)
    {
        ttns_free_mix_buffers();
        return 1;
    }

    return 0;
}

static int ttns_peak_apply_gain(int peak, float gain)
{
    if (peak <= 0 || gain <= 0.0f)
        return 0;

    return (int)((float)peak * gain);
}

static void ttns_push_fader_meters(int line_pk_raw, int mic_pk_raw, int cart_pk_raw)
{
    int line_meter = ttns_peak_apply_gain(line_pk_raw, cfg.ttns.line_gain);
    int mic_meter = ttns_peak_apply_gain(mic_pk_raw, cfg.ttns.mic_gain);
    int cart_meter = ttns_peak_apply_gain(cart_pk_raw, cfg.ttns.cart_gain);

    ttns_meters_push(line_meter, mic_meter, cart_meter);
}

static int ttns_peak_stereo(const short *stereo, int frameCount)
{
    int fi;
    int pk = 0;

    for (fi = 0; fi < frameCount; fi++)
    {
        int l = abs(stereo[fi * 2]);
        int r = abs(stereo[fi * 2 + 1]);

        if (l > pk)
            pk = l;
        if (r > pk)
            pk = r;
    }

    return pk;
}

static int ttns_device_input_channels(int dev_num)
{
    const PaDeviceInfo *info;
    PaDeviceIndex pa_id;

    if (dev_num < 0 || dev_num >= cfg.audio.dev_count)
        return 1;

    pa_id = cfg.audio.pcm_list[dev_num]->dev_id;
    info = Pa_GetDeviceInfo(pa_id);
    if (!info || info->maxInputChannels < 1)
        return 1;

    return info->maxInputChannels;
}

static int ttns_pick_open_channels(int dev_num, int want)
{
    int max_ch = ttns_device_input_channels(dev_num);

    if (want > max_ch)
        want = max_ch;
    if (want < 1)
        want = 1;

    return want;
}

static int ttns_mic_capture_cb(const void *inputBuffer, void *outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo *timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData)
{
    size_t bytes;

    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (!inputBuffer || framesPerBuffer == 0)
        return paContinue;

    if (framesPerBuffer > (unsigned long)pa_frames)
        framesPerBuffer = (unsigned long)pa_frames;

    if (ttns_mic_rb_inited)
    {
        bytes = framesPerBuffer * (size_t)mic_input_channels * sizeof(short);
        rb_write_drop(&ttns_mic_rb, (char*)inputBuffer, (unsigned int)bytes);
    }

    if (snd_audio_active)
    {
        mic_capture_peak = ttns_mic_peak((const short*)inputBuffer, mic_input_channels,
                                         (int)framesPerBuffer);
    }

    return paContinue;
}

static void ttns_copy_line_to_stereo(short *line_stereo, const short *line_in,
                                     int frameCount, int in_channels)
{
    int i;

    for (i = 0; i < frameCount; i++)
    {
        if (in_channels >= 2)
        {
            line_stereo[i * 2] = line_in[i * 2];
            line_stereo[i * 2 + 1] = line_in[i * 2 + 1];
        }
        else
        {
            line_stereo[i * 2] = line_in[i];
            line_stereo[i * 2 + 1] = line_in[i];
        }
    }
}

static void ttns_finish_mix_block(int frameCount, const short *line_stereo,
                                  const short *mic_in, int mic_channels)
{
    int mic_pk_raw;
    int duck_pk;
    int r;
    float duck_depth;
    float duck_gain;
    float mic_g;
    const short *remote_stereo[TTNS_REMOTE_SLOTS];
    float remote_gain[TTNS_REMOTE_SLOTS];

    ttns_remote_prepare_block(frameCount);
    ttns_gather_remote_voices(remote_stereo, remote_gain);

    mic_pk_raw = ttns_mic_peak(mic_in, mic_channels, frameCount);
    duck_pk = ttns_mic_effective_mute() ? 0 : mic_pk_raw;
    {
        int rem_pk = ttns_remote_duck_peak();
        if (rem_pk > duck_pk)
            duck_pk = rem_pk;
    }

    duck_depth = util_db_to_factor(cfg.ttns.duck_depth_db);
    duck_gain = ttns_duck_gain_update(duck_pk,
                                      cfg.audio.samplerate, frameCount,
                                      cfg.ttns.duck_threshold, duck_depth,
                                      cfg.ttns.duck_attack_ms, cfg.ttns.duck_release_ms);

    mic_g = cfg.ttns.mic_gain;
    if (ttns_mic_effective_mute())
        mic_g = 0.0f;

    ttns_process_mix_ex(ttns_mix_buf, mic_in, mic_channels, line_stereo, ttns_cart_buf,
                        frameCount, mic_g, cfg.ttns.line_gain, cfg.ttns.cart_gain, duck_gain,
                        remote_stereo, remote_gain, -1);
    memcpy(pa_pcm_buf, ttns_mix_buf, (size_t)frameCount * 2 * sizeof(short));

    if (ttns_mix_minus_buf)
    {
        for (r = 0; r < TTNS_REMOTE_SLOTS; r++)
        {
            if (!ttns_remote_is_live(r))
                continue;
            ttns_process_mix_ex(ttns_mix_minus_buf, mic_in, mic_channels, line_stereo,
                                ttns_cart_buf, frameCount, mic_g, cfg.ttns.line_gain,
                                cfg.ttns.cart_gain, duck_gain,
                                remote_stereo, remote_gain, r);
            ttns_remote_write_mix_minus(r, ttns_mix_minus_buf, frameCount);
        }
    }

    ttns_push_monitor_mix(frameCount, line_stereo, mic_in, mic_channels,
                          cfg.ttns.line_gain, cfg.ttns.cart_gain, duck_gain, mic_g,
                          remote_stereo, remote_gain);

    ttns_push_fader_meters(ttns_peak_stereo(line_stereo, frameCount), mic_pk_raw,
                           ttns_peak_stereo(ttns_cart_buf, frameCount));
}

int snd_init(void)
{
    char info_buf[256];

    PaError p_err;
    if((p_err = Pa_Initialize()) != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf),
				"PortAudio init failed:\n%s\n",
				Pa_GetErrorText(p_err));

        ALERT(info_buf);
        return 1;
    }

    srconv_stream_in_buf = (float*)malloc(pa_frames*2 * sizeof(float));
    srconv_stream_out_buf = (float*)malloc(pa_frames*2 * 10 * sizeof(float));
    srconv_record_in_buf = (float*)malloc(pa_frames*2 * sizeof(float));
    srconv_record_out_buf = (float*)malloc(pa_frames*2 * 10 * sizeof(float));
    srconv_stream.data_in = srconv_stream_in_buf;
    srconv_stream.data_out = srconv_stream_out_buf;
    srconv_record.data_in = srconv_record_in_buf;
    srconv_record.data_out = srconv_record_out_buf;

    reconnect = 0;
    buf_index = 0;
    return 0;
}

static void snd_stop_streams(void);
static void snd_close_monitor(void);

void snd_stop_input(void)
{
    snd_stop_streams();
}

int snd_audio_is_active(void)
{
    return snd_audio_active;
}

static void snd_pause_after_stop(void)
{
#ifdef _WIN32
    Pa_Sleep(50);
#elif defined(__APPLE__)
    Pa_Sleep(200);
#endif
}

void snd_reinit(void)
{
    snd_stop_streams();
    snd_pause_after_stop();
    snd_open_stream();
}

static void snd_abort_open(void)
{
    snd_stop_streams();
}

static void snd_stop_mic_capture(void)
{
    if (mic_stream != NULL)
    {
        Pa_StopStream(mic_stream);
        Pa_CloseStream(mic_stream);
        mic_stream = NULL;
    }

    ttns_mic_rb_shutdown();
    ttns_use_dual_mic = 0;
    mic_capture_peak = 0;
    ttns_meters_reset_mic();
}

static int snd_run_mic_capture(void)
{
    char info_buf[256];
    int mic_dev_num = cfg.ttns.mic_dev_num;
    PaError pa_err;

    if (!mic_stream)
        return 1;

    pa_err = Pa_StartStream(mic_stream);
    if (pa_err != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf),
                 "Mic capture start failed: %s", Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        Pa_CloseStream(mic_stream);
        mic_stream = NULL;
        ttns_mic_rb_shutdown();
        ttns_use_dual_mic = 0;
        return 1;
    }

    if (mic_dev_num < 0 || mic_dev_num >= cfg.audio.dev_count)
        mic_dev_num = cfg.audio.dev_num;
    snprintf(info_buf, sizeof(info_buf), "Mic capture: %s",
             cfg.audio.pcm_list[mic_dev_num]->name);
    print_info(info_buf, 0);
    return 0;
}

static int snd_open_mic_capture(int samplerate)
{
    char info_buf[256];
    int line_dev_num = cfg.ttns.line_dev_num;
    int mic_dev_num = cfg.ttns.mic_dev_num;
    PaStreamParameters mic_params;
    const PaDeviceInfo *mic_dev_info;
    PaDeviceIndex mic_pa_dev_id;
    PaError pa_err;

    if (line_dev_num < 0 || line_dev_num >= cfg.audio.dev_count)
        line_dev_num = cfg.audio.dev_num;
    if (mic_dev_num < 0 || mic_dev_num >= cfg.audio.dev_count)
        mic_dev_num = cfg.audio.dev_num;

    if (mic_dev_num == line_dev_num)
        return 1;

    mic_pa_dev_id = cfg.audio.pcm_list[mic_dev_num]->dev_id;
    mic_dev_info = Pa_GetDeviceInfo(mic_pa_dev_id);
    if (mic_dev_info == NULL)
    {
        snprintf(info_buf, sizeof(info_buf), "Error getting mic device info (%d)", mic_pa_dev_id);
        print_info(info_buf, 1);
        return 1;
    }

    /* Mono avoids channel/format issues on virtual devices (SplitCam, Zoom, etc.). */
    mic_input_channels = 1;
    if (mic_dev_info->maxInputChannels < 1)
        return 1;

    mic_params.device = mic_pa_dev_id;
    mic_params.channelCount = mic_input_channels;
    mic_params.sampleFormat = paInt16;
    mic_params.suggestedLatency = mic_dev_info->defaultHighInputLatency;
    mic_params.hostApiSpecificStreamInfo = NULL;

    pa_err = Pa_IsFormatSupported(&mic_params, NULL, samplerate);
    if (pa_err != paFormatIsSupported)
    {
        snprintf(info_buf, sizeof(info_buf),
                 "Mic device format not supported at %d Hz — check Settings → Audio",
                 samplerate);
        print_info(info_buf, 1);
        return 1;
    }

    pa_err = Pa_OpenStream(&mic_stream, &mic_params, NULL,
                           samplerate, pa_frames,
                           paClipOff, ttns_mic_capture_cb, NULL);
    if (pa_err != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf),
                 "Mic device open failed: %s", Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        return 1;
    }

    if (ttns_mic_rb_start() != 0)
    {
        Pa_CloseStream(mic_stream);
        mic_stream = NULL;
        print_info("Mic capture: out of memory", 1);
        return 1;
    }

    ttns_use_dual_mic = 1;
    mic_capture_peak = 0;
    return 0;
}

static int snd_start_mic_capture(int samplerate)
{
    if (snd_open_mic_capture(samplerate) != 0)
        return 1;
    return snd_run_mic_capture();
}

int snd_reopen_mic_only(void)
{
    int line_dev_num = cfg.ttns.line_dev_num;
    int mic_dev_num = cfg.ttns.mic_dev_num;

    if (stream == NULL)
        return snd_open_stream();

    if (line_dev_num < 0 || line_dev_num >= cfg.audio.dev_count)
        line_dev_num = cfg.audio.dev_num;
    if (mic_dev_num < 0 || mic_dev_num >= cfg.audio.dev_count)
        mic_dev_num = cfg.audio.dev_num;

    if (mic_dev_num == line_dev_num)
    {
        snd_reinit();
        return 0;
    }

    snd_stop_mic_capture();
#ifdef __APPLE__
    Pa_Sleep(300);
#elif defined(_WIN32)
    Pa_Sleep(100);
#endif

    if (snd_open_mic_capture(cfg.audio.samplerate) != 0)
        return 1;

    return snd_run_mic_capture();
}

static void snd_stop_streams(void)
{
    snd_audio_active = 0;
    pa_new_frames = 0;

    if (monitor_stream != NULL)
    {
        Pa_StopStream(monitor_stream);
        Pa_CloseStream(monitor_stream);
        monitor_stream = NULL;
    }

    snd_stop_mic_capture();

    if (stream != NULL)
    {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = NULL;
    }

    ttns_monitor_rb_shutdown();
    free(mic_pcm_buf);
    mic_pcm_buf = NULL;
    ttns_free_mix_buffers();
    rb_free(&rec_rb);
    rb_free(&stream_rb);
    free(snd_conv_work_buf);
    snd_conv_work_buf = NULL;
    free(pa_pcm_buf);
    pa_pcm_buf = NULL;
    free(encode_buf);
    encode_buf = NULL;
    ttns_use_dual_mic = 0;
    ttns_use_shared_input = 0;
    mic_capture_peak = 0;
    ttns_meters_reset();
}

static void snd_close_monitor(void)
{
    if (monitor_stream != NULL)
    {
        Pa_StopStream(monitor_stream);
        Pa_CloseStream(monitor_stream);
        monitor_stream = NULL;
    }

    ttns_monitor_rb_shutdown();
}

static int snd_open_monitor(int samplerate)
{
    char info_buf[256];
    PaStreamParameters out_params;
    PaDeviceIndex out_dev;
    const PaDeviceInfo *out_info;
    PaError pa_err;

    snd_close_monitor();

    if (!cfg.ttns.mic_monitor)
        return 0;

    if (cfg.audio.out_dev_count <= 0)
        return 0;

    {
        int out_dev_num = cfg.ttns.monitor_out_dev_num;

        if (out_dev_num < 0 || out_dev_num >= cfg.audio.out_dev_count)
            out_dev_num = 0;

        if (cfg.audio.out_dev_count > 0 && cfg.audio.out_pcm_list != NULL)
            out_dev = cfg.audio.out_pcm_list[out_dev_num]->dev_id;
        else
            out_dev = Pa_GetDefaultOutputDevice();
    }

    if (out_dev == TTNS_MONITOR_OFF)
        return 0;

    if (out_dev == paNoDevice)
    {
        print_info("Mic monitor: no output device", 1);
        return 1;
    }

    out_info = Pa_GetDeviceInfo(out_dev);
    if (out_info == NULL)
        return 1;

    out_params.device = out_dev;
    out_params.channelCount = 2;
    out_params.sampleFormat = paInt16;
    out_params.suggestedLatency = out_info->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = NULL;

    pa_err = Pa_IsFormatSupported(NULL, &out_params, samplerate);
    if (pa_err != paFormatIsSupported)
    {
        print_info("Mic monitor: output format not supported", 1);
        return 1;
    }

    if (ttns_monitor_rb_start() != 0)
    {
        print_info("Mic monitor: out of memory", 1);
        return 1;
    }

    pa_err = Pa_OpenStream(&monitor_stream, NULL, &out_params,
                           samplerate, pa_frames, paClipOff, monitor_out_cb, NULL);
    if (pa_err != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf), "Mic monitor open failed: %s",
                 Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        return 1;
    }

    pa_err = Pa_StartStream(monitor_stream);
    if (pa_err != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf), "Mic monitor start failed: %s",
                 Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        Pa_CloseStream(monitor_stream);
        monitor_stream = NULL;
        return 1;
    }

    {
        int out_dev_num = cfg.ttns.monitor_out_dev_num;

        if (out_dev_num < 0 || out_dev_num >= cfg.audio.out_dev_count)
            out_dev_num = 0;
        if (cfg.audio.out_dev_count > 0 && cfg.audio.out_pcm_list != NULL)
        {
            snprintf(info_buf, sizeof(info_buf), "Mic monitor: %s",
                     cfg.audio.out_pcm_list[out_dev_num]->name);
            print_info(info_buf, 0);
        }
    }

    return 0;
}

void snd_reopen_monitor(void)
{
    PaError pa_err;

    if (!stream || !snd_audio_active)
    {
        snd_close_monitor();
        return;
    }

    Pa_StopStream(stream);
    if (mic_stream != NULL)
        Pa_StopStream(mic_stream);

    snd_close_monitor();

    if (snd_open_monitor(cfg.audio.samplerate) != 0)
    {
        if (mic_stream != NULL)
            Pa_StartStream(mic_stream);
        Pa_StartStream(stream);
        return;
    }

    if (mic_stream != NULL)
    {
        pa_err = Pa_StartStream(mic_stream);
        if (pa_err != paNoError)
            print_info("Mic capture restart failed after monitor change", 1);
    }

    pa_err = Pa_StartStream(stream);
    if (pa_err != paNoError)
        print_info("Audio input restart failed after monitor change", 1);
}

int snd_open_stream(void)
{

    int samplerate;
    char info_buf[256];

    PaDeviceIndex pa_dev_id;
    PaStreamParameters pa_params;
    PaError pa_err;
    const PaDeviceInfo *pa_dev_info;

    if (stream != NULL || mic_stream != NULL || monitor_stream != NULL)
    {
        snd_stop_streams();
        snd_pause_after_stop();
    }

    if(cfg.audio.dev_count == 0)
    {
        print_info("ERROR: no sound device with input channels found", 1);
        return 1;
    }


    pa_frames = (cfg.audio.buffer_ms/1000.0)*cfg.audio.samplerate;
    if (pa_frames < 256)
        pa_frames = 256;
    if (pa_frames > 8192)
        pa_frames = 8192;

    snd_reset_samplerate_conv(SND_STREAM);
    snd_reset_samplerate_conv(SND_REC);


    framepacket_size = pa_frames * cfg.audio.channel;

    free(pa_pcm_buf);
    pa_pcm_buf = NULL;
    free(encode_buf);
    encode_buf = NULL;
    free(snd_conv_work_buf);
    snd_conv_work_buf = NULL;

    pa_pcm_buf = (short*)malloc(16 * framepacket_size * sizeof(short));
    encode_buf = (char*)malloc(16 * framepacket_size * sizeof(char));
    snd_conv_work_buf = (short*)malloc(16 * pa_frames * 2 * sizeof(short));

    if (!pa_pcm_buf || !encode_buf || !snd_conv_work_buf)
    {
        print_info("ERROR: Out of memory opening audio", 1);
        snd_abort_open();
        return 1;
    }

    if (ttns_alloc_mix_buffers() != 0)
    {
        print_info("ERROR: Out of memory opening audio buffers", 1);
        snd_abort_open();
        return 1;
    }

    rb_init(&rec_rb, 16 * framepacket_size * sizeof(short));
    rb_init(&stream_rb, 16 * framepacket_size * sizeof(short));

    samplerate = cfg.audio.samplerate;

    {
        int line_dev_num = cfg.ttns.line_dev_num;
        if (line_dev_num < 0 || line_dev_num >= cfg.audio.dev_count)
            line_dev_num = cfg.audio.dev_num;
        pa_dev_id = cfg.audio.pcm_list[line_dev_num]->dev_id;
    }

    pa_dev_info = Pa_GetDeviceInfo(pa_dev_id);
    if(pa_dev_info == NULL)
    {
        snprintf(info_buf, 127, "Error getting device Info (%d)", pa_dev_id);
        print_info(info_buf, 1);
        snd_abort_open();
        return 1;
    }

    pa_params.device = pa_dev_id;
    line_input_channels = ttns_pick_open_channels(
        (cfg.ttns.line_dev_num >= 0 && cfg.ttns.line_dev_num < cfg.audio.dev_count)
            ? cfg.ttns.line_dev_num : cfg.audio.dev_num,
        cfg.audio.channel);
    pa_params.channelCount = line_input_channels;
    pa_params.sampleFormat = paInt16;
    pa_params.suggestedLatency = pa_dev_info->defaultHighInputLatency;
    pa_params.hostApiSpecificStreamInfo = NULL;

    pa_err = Pa_IsFormatSupported(&pa_params, NULL, samplerate);
    if(pa_err != paFormatIsSupported)
    {
        if (line_input_channels > 1)
        {
            line_input_channels = 1;
            pa_params.channelCount = 1;
            pa_err = Pa_IsFormatSupported(&pa_params, NULL, samplerate);
        }

        if(pa_err != paFormatIsSupported && pa_err == paInvalidSampleRate)
        {
            snprintf(info_buf, sizeof(info_buf),
                    "Samplerate not supported: %dHz\n"
                    "Using default samplerate: %dHz",
                    samplerate, (int)pa_dev_info->defaultSampleRate);
            print_info(info_buf, 1);

            if(Pa_IsFormatSupported(&pa_params, NULL,
               pa_dev_info->defaultSampleRate) != paFormatIsSupported)
            {
                print_info("FAILED", 1);
                snd_abort_open();
                return 1;
            }
            else
            {
                samplerate = (int)pa_dev_info->defaultSampleRate;
                cfg.audio.samplerate = samplerate;
                update_samplerates();
            }
        }
        else
        {
            snprintf(info_buf, sizeof(info_buf), "PA: Format not supported: %s\n",
                    Pa_GetErrorText(pa_err));
            print_info(info_buf, 1);
            snd_abort_open();
            return 1;
        }
    }

    stream = NULL;
    pa_err = Pa_OpenStream(&stream, &pa_params, NULL,
                            samplerate, pa_frames,
                            paNoFlag, snd_callback, NULL);

    if(pa_err != paNoError)
    {
        snprintf(info_buf, sizeof(info_buf),
                 "error opening sound device: %s", Pa_GetErrorText(pa_err));
        print_info(info_buf, 1);
        snd_abort_open();
        return 1;
    }

    snd_audio_active = 0;
    ttns_use_dual_mic = 0;
    ttns_use_shared_input = 0;
    mic_stream = NULL;

    {
        int line_dev_num = cfg.ttns.line_dev_num;
        int mic_dev_num = cfg.ttns.mic_dev_num;

        if (line_dev_num < 0 || line_dev_num >= cfg.audio.dev_count)
            line_dev_num = cfg.audio.dev_num;
        if (mic_dev_num < 0 || mic_dev_num >= cfg.audio.dev_count)
            mic_dev_num = cfg.audio.dev_num;

        if (mic_dev_num != line_dev_num)
        {
            if (snd_open_mic_capture(samplerate) != 0)
                print_info("Mic capture inactive — deck-only mode", 1);
        }
        else
        {
            int line_ch = ttns_device_input_channels(line_dev_num);

            ttns_use_shared_input = 1;
            mic_pcm_buf = (short*)malloc(pa_frames * 2 * sizeof(short));
            if (!mic_pcm_buf)
            {
                print_info("Shared input: out of memory", 1);
                ttns_use_shared_input = 0;
            }
            else
            {
                mic_input_channels = 1;
                if (line_ch >= 2)
                    print_info("TTNS: Deck and Mic share one device (stereo: L=mic, R=deck)", 0);
                else
                    print_info("TTNS: Deck and Mic share one mono device — use separate devices to split mic from deck", 1);
            }
        }
    }

    snd_open_monitor(samplerate);

    if (Pa_StartStream(stream) != paNoError)
    {
        print_info("ERROR: Could not start audio input stream", 1);
        snd_abort_open();
        return 1;
    }

    if (ttns_use_dual_mic && mic_stream != NULL)
    {
        if (snd_run_mic_capture() != 0)
            print_info("Mic capture inactive — deck-only mode", 1);
    }

    snd_audio_active = 1;

    {
        int line_dev_num = cfg.ttns.line_dev_num;
        int mic_dev_num = cfg.ttns.mic_dev_num;

        if (line_dev_num < 0 || line_dev_num >= cfg.audio.dev_count)
            line_dev_num = cfg.audio.dev_num;
        if (mic_dev_num < 0 || mic_dev_num >= cfg.audio.dev_count)
            mic_dev_num = cfg.audio.dev_num;

        if (ttns_use_dual_mic)
        {
            snprintf(info_buf, sizeof(info_buf),
                     "TTNS audio: deck=%s (%d ch), mic=%s (%d ch)",
                     cfg.audio.pcm_list[line_dev_num]->name, line_input_channels,
                     cfg.audio.pcm_list[mic_dev_num]->name, mic_input_channels);
        }
        else if (ttns_use_shared_input)
        {
            snprintf(info_buf, sizeof(info_buf),
                     "TTNS audio: shared device %s (stereo L=mic, R=deck)",
                     cfg.audio.pcm_list[line_dev_num]->name);
        }
        else
        {
            snprintf(info_buf, sizeof(info_buf),
                     "TTNS audio: deck=%s (%d ch) — mic capture inactive",
                     cfg.audio.pcm_list[line_dev_num]->name, line_input_channels);
        }
        print_info(info_buf, 0);
    }

    return 0;
}

void snd_start_stream(void)
{   
    pthread_mutex_init(&stream_mut, NULL);
    pthread_cond_init (&stream_cond, NULL);
    
    kbytes_sent = 0;
    streaming = 1;

    pthread_create(&stream_thread, NULL, snd_stream_thread, NULL);
}

void snd_stop_stream(void)
{
    connected = 0;
    streaming = 0;

    pthread_cond_signal(&stream_cond);

    pthread_mutex_destroy(&stream_mut);
    pthread_cond_destroy(&stream_cond);


    print_info("user disconnected\n", 0);
}

void *snd_stream_thread(void *data)
{
    int sent;
    int rb_bytes_read;
	int encode_bytes_read = 0;
    int bytes_to_read;

    char *enc_buf = (char*)malloc(stream_rb.size * sizeof(char)*10);
    char *audio_buf = (char*)malloc(stream_rb.size * sizeof(char)*10);

    int (*xc_send)(char *buf, int buf_len) = NULL;



    if(cfg.srv[cfg.selected_srv]->type == SHOUTCAST)
        xc_send = &sc_send;
    else //Icecast
        xc_send = &ic_send;

    FILE *fd;

    while(connected)
    {

        pthread_cond_wait(&stream_cond, &stream_mut);
        if(!connected)
            break;

        if(!strcmp(cfg.audio.codec, "opus"))
        {
            // Read always chunks of 960 frames from the audio ringbuffer to be
            // compatible with OPUS
            bytes_to_read = 960 * sizeof(short)*cfg.audio.channel;
            
            while ((rb_filled(&stream_rb)) >= bytes_to_read)
            {
                // Read always chunks of 960 frames from the audio ringbuffer to be
                bytes_to_read = 960 * sizeof(short)*cfg.audio.channel;
                rb_read_len(&stream_rb, audio_buf, bytes_to_read);

                encode_bytes_read = opus_enc_encode(&opus_stream, (short*)audio_buf,
                        enc_buf, bytes_to_read/(2*cfg.audio.channel));

                if((sent = xc_send(enc_buf, encode_bytes_read)) == -1)
                {
                    connected = 0;
                }
                else
                    kbytes_sent += bytes_to_read/1024.0;

            }
        }
        else if(!strcmp(cfg.audio.codec, "aac"))
        {
            bytes_to_read = aac_stream.info.frameLength * cfg.audio.channel * sizeof(short);
            while ((rb_filled(&stream_rb)) >= bytes_to_read)
            {
                rb_read_len(&stream_rb, audio_buf, bytes_to_read);

                encode_bytes_read = aac_enc_encode(&aac_stream, (short*)audio_buf, enc_buf,
                        bytes_to_read/(2*cfg.audio.channel), stream_rb.size*10);

                if((sent = xc_send(enc_buf, encode_bytes_read)) == -1)
                {
                    connected = 0;
                }
                else
                    kbytes_sent += bytes_to_read/1024.0;

            }
        }
        else // ogg and mp3 need more data than opus in order to compress the audio data
        {
            if(rb_filled(&stream_rb) < framepacket_size*sizeof(short))
                continue;

            rb_bytes_read = rb_read(&stream_rb, audio_buf);
            if(rb_bytes_read == 0)
                continue;

            if(!strcmp(cfg.audio.codec, "mp3"))
                encode_bytes_read = lame_enc_encode(&lame_stream, (short*)audio_buf, enc_buf,
                        rb_bytes_read/(2*cfg.audio.channel), stream_rb.size*10);
            
            if(!strcmp(cfg.audio.codec, "ogg"))
                encode_bytes_read = vorbis_enc_encode(&vorbis_stream, (short*)audio_buf, 
                        enc_buf, rb_bytes_read/(2*cfg.audio.channel));

            if((sent = xc_send(enc_buf, encode_bytes_read)) == -1)
                connected = 0; 
            else
                kbytes_sent += encode_bytes_read/1024.0;

        }
    }

    free(enc_buf);
    free(audio_buf);

    return NULL;
}

void snd_start_rec(void)
{
    int error;
    next_file = 0;

    kbytes_written = 0;
    recording = 1;
    
    pthread_mutex_init(&rec_mut, NULL);
    pthread_cond_init (&rec_cond, NULL);

    pthread_create(&rec_thread, NULL, snd_rec_thread, NULL);

    print_info("recording to:", 0);
    print_info(cfg.rec.path, 0);
}

void snd_stop_rec(void)
{
    record = 0;
    recording = 0;
    

    pthread_cond_signal(&rec_cond);

    pthread_mutex_destroy(&rec_mut);
    pthread_cond_destroy(&rec_cond);

    print_info("recording stopped", 0);
}

//The recording stuff runs in its own thread
//this prevents dropouts in the recording in case the
//bandwidth is smaller than the selected streaming bitrate
void* snd_rec_thread(void *data)
{
    int error;
    int rb_bytes_read;
    int bytes_to_read;
    int ogg_header_written;
    int opus_header_written;
    int enc_bytes_read;
    char info_buf[256];
    
    char *enc_buf = (char*)malloc(rec_rb.size * sizeof(char)*10);
    char *audio_buf = (char*)malloc(rec_rb.size * sizeof(char)*10);
    
    ogg_header_written = 0;
    opus_header_written = 0;

    while(record)
    {
        pthread_cond_wait(&rec_cond, &rec_mut);

        if(next_file == 1)
        {
            if(!strcmp(cfg.rec.codec, "flac")) // The flac encoder closes the file
                flac_enc_close(&flac_rec);
            else
                fclose(cfg.rec.fd);

            cfg.rec.fd = next_fd;
            next_file = 0;
            if(!strcmp(cfg.rec.codec, "ogg"))
            {
                vorbis_enc_reinit(&vorbis_rec);
                ogg_header_written = 0;
            }
            if(!strcmp(cfg.rec.codec, "opus"))
            {
                opus_enc_reinit(&opus_rec);
                opus_header_written = 0;
            }
            if(!strcmp(cfg.rec.codec, "flac"))
            {
                flac_enc_reinit(&flac_rec);
                flac_enc_init_FILE(&flac_rec, cfg.rec.fd);
            }
        }

        // Opus and aac need  special treatments
        // The encoders need a predefined count of frames
        // Therefore we don't feed the encoder with all data we have in the
        // ringbuffer at once 
        if(!strcmp(cfg.rec.codec, "opus"))
        {
            bytes_to_read = 960 * sizeof(short)*cfg.audio.channel;
            while ((rb_filled(&rec_rb)) >= bytes_to_read)
            {
                rb_read_len(&rec_rb, audio_buf, bytes_to_read);

                if(!opus_header_written)
                {
                    opus_enc_write_header(&opus_rec);
                    opus_header_written = 1;
                }

                enc_bytes_read = opus_enc_encode(&opus_rec, (short*)audio_buf, 
                        enc_buf, bytes_to_read/(2*cfg.audio.channel));
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd)/1024.0;
            }
        }
        else if(!strcmp(cfg.rec.codec, "aac"))
        {

            bytes_to_read = aac_rec.info.frameLength * cfg.audio.channel * sizeof(short);
            while ((rb_filled(&rec_rb)) >= bytes_to_read)
            {
                rb_read_len(&rec_rb, audio_buf, bytes_to_read);

                enc_bytes_read = aac_enc_encode(&aac_rec, (short*)audio_buf, 
                        enc_buf, bytes_to_read/(2*cfg.audio.channel), rec_rb.size * sizeof(char)*10);
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd)/1024.0;
            }
        }
        else
        {

            if(rb_filled(&rec_rb) < framepacket_size*sizeof(short))
                continue;

            rb_bytes_read = rb_read(&rec_rb, audio_buf);
            if(rb_bytes_read == 0)
                continue;


            if(!strcmp(cfg.rec.codec, "mp3"))
            {

                enc_bytes_read = lame_enc_encode(&lame_rec, (short*)audio_buf, enc_buf,
                        rb_bytes_read/(2*cfg.audio.channel), rec_rb.size*10);
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd)/1024.0;
            }

            if(!strcmp(cfg.rec.codec, "ogg"))
            {
                if(!ogg_header_written)
                {
                    vorbis_enc_write_header(&vorbis_rec);
                    ogg_header_written = 1;
                }

                enc_bytes_read = vorbis_enc_encode(&vorbis_rec, (short*)audio_buf, 
                        enc_buf, rb_bytes_read/(2*cfg.audio.channel));
                kbytes_written += fwrite(enc_buf, 1, enc_bytes_read, cfg.rec.fd)/1024.0;
            }

            if(!strcmp(cfg.rec.codec, "flac"))
            {
                flac_enc_encode(&flac_rec, (short*)audio_buf, rb_bytes_read/sizeof(short)/cfg.audio.channel, cfg.audio.channel);
                kbytes_written = flac_enc_get_bytes_written()/1024.0;
            }


            if(!strcmp(cfg.rec.codec, "wav"))
            {
                //this permanently updates the filesize value in the WAV header
                //so we still have a valid WAV file in case of a crash
                wav_write_header(cfg.rec.fd, cfg.audio.channel, cfg.audio.samplerate, 16);
                kbytes_written += fwrite(audio_buf, sizeof(char), rb_bytes_read, cfg.rec.fd)/1024.0;
            }
        }
    }

    if(!strcmp(cfg.rec.codec, "flac"))  // The flac encoder closes the file
        flac_enc_close(&flac_rec);
    else
        fclose(cfg.rec.fd);
    
    free(enc_buf);
    free(audio_buf);
    
    return NULL;
}

//this function is called by PortAudio when new audio data arrived
int snd_callback(const void *input,
                 void *output,
                 unsigned long frameCount,
                 const PaStreamCallbackTimeInfo* timeInfo,
                 PaStreamCallbackFlags statusFlags,
                 void *userData)
{
    int i;
    int error;
    int samplerate_out;
    bool convert_stream = false;
    bool convert_record = false;

    (void)output;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (!snd_audio_active || !pa_pcm_buf || !ttns_cart_buf || !ttns_line_buf)
        return paContinue;

    if (frameCount == 0)
        return paContinue;

    if (frameCount > (unsigned long)pa_frames)
        frameCount = (unsigned long)pa_frames;

    if (ttns_use_dual_mic && mic_stream != NULL && ttns_mix_buf != NULL && ttns_mic_rb_inited)
    {
        const short *line_in = (const short*)input;
        const short *mic_in;
        short *mic_copy = ttns_mic_work_buf;

        ttns_mic_rb_read(mic_copy, (int)frameCount, mic_input_channels);
        mic_in = mic_copy;

        ttns_cart_render(ttns_cart_buf, (int)frameCount);

        ttns_copy_line_to_stereo(ttns_line_buf, line_in, (int)frameCount, line_input_channels);

        ttns_finish_mix_block((int)frameCount, ttns_line_buf, mic_in, mic_input_channels);
    }
    else if (ttns_use_shared_input && ttns_mix_buf != NULL && mic_pcm_buf != NULL)
    {
        const short *in = (const short*)input;
        const short *mic_in;
        int mic_ch;

        ttns_cart_render(ttns_cart_buf, (int)frameCount);

        if (cfg.audio.channel >= 2 && line_input_channels >= 2)
        {
            for (i = 0; i < (int)frameCount; i++)
            {
                mic_pcm_buf[i] = in[i * 2];
                ttns_line_buf[i * 2] = in[i * 2 + 1];
                ttns_line_buf[i * 2 + 1] = in[i * 2 + 1];
            }
            mic_in = mic_pcm_buf;
            mic_ch = 1;
        }
        else
        {
            for (i = 0; i < (int)frameCount; i++)
            {
                ttns_line_buf[i * 2] = in[i];
                ttns_line_buf[i * 2 + 1] = in[i];
            }
            mic_in = in;
            mic_ch = 1;
        }

        ttns_finish_mix_block((int)frameCount, ttns_line_buf, mic_in, mic_ch);
    }
    else
    {
        const short *line_in = (const short*)input;
        static short silent_mic[8192];

        ttns_cart_render(ttns_cart_buf, (int)frameCount);
        ttns_copy_line_to_stereo(ttns_line_buf, line_in, (int)frameCount, line_input_channels);

        if (ttns_mix_buf && frameCount <= 8192)
        {
            memset(silent_mic, 0, (size_t)frameCount * sizeof(short));
            ttns_finish_mix_block((int)frameCount, ttns_line_buf, silent_mic, 1);
        }
        else if (!ttns_mix_buf)
        {
            for (i = 0; i < (int)frameCount; i++)
            {
                int l = ttns_line_buf[i * 2];
                int r = ttns_line_buf[i * 2 + 1];
                int cl = ttns_cart_buf[i * 2];
                int cr = ttns_cart_buf[i * 2 + 1];
                float lg = cfg.ttns.line_gain;
                float cg = cfg.ttns.cart_gain;

                pa_pcm_buf[i * 2] = ttns_clamp16((int)(l * lg + cl * cg));
                pa_pcm_buf[i * 2 + 1] = ttns_clamp16((int)(r * lg + cr * cg));
            }
        }
        else
        {
            const short *remote_stereo[TTNS_REMOTE_SLOTS];
            float remote_gain[TTNS_REMOTE_SLOTS];

            ttns_remote_prepare_block((int)frameCount);
            ttns_gather_remote_voices(remote_stereo, remote_gain);
            ttns_push_monitor_mix((int)frameCount, ttns_line_buf, NULL, 1,
                                  cfg.ttns.line_gain, cfg.ttns.cart_gain, 1.0f, 0.0f,
                                  remote_stereo, remote_gain);
            ttns_push_fader_meters(ttns_peak_stereo(ttns_line_buf, (int)frameCount), 0,
                                   ttns_peak_stereo(ttns_cart_buf, (int)frameCount));
        }
    }
    samplerate_out = cfg.audio.samplerate;
	
	if (streaming)
	{
        if ((!strcmp(cfg.audio.codec, "opus")) && (cfg.audio.samplerate != 48000))
        {
            convert_stream = true;
            samplerate_out = 48000;
        }

        if (convert_stream == true)
        {
            srconv_stream.end_of_input = 0;
            srconv_stream.src_ratio = (float)samplerate_out/cfg.audio.samplerate;
            srconv_stream.input_frames = frameCount;
            srconv_stream.output_frames = frameCount*cfg.audio.channel * (srconv_stream.src_ratio+1) * sizeof(float);

            src_short_to_float_array((short*)pa_pcm_buf, srconv_stream_in_buf, frameCount*cfg.audio.channel);

            //The actual resample process
            src_process(srconv_state_stream, &srconv_stream);

            src_float_to_short_array(srconv_stream.data_out, snd_conv_work_buf,
                                     srconv_stream.output_frames_gen*cfg.audio.channel);

            rb_write(&stream_rb, (char*)snd_conv_work_buf,
                     srconv_stream.output_frames_gen*sizeof(short)*cfg.audio.channel);
        }
        else
            rb_write(&stream_rb, (char*)pa_pcm_buf, frameCount*sizeof(short)*cfg.audio.channel);

		pthread_cond_signal(&stream_cond);
	}

	if(recording)
	{

        if ((!strcmp(cfg.rec.codec, "opus")) && (cfg.audio.samplerate != 48000))
        {
            convert_record = true;
            samplerate_out = 48000;
        }

        if (convert_record == true)
        {
            srconv_record.end_of_input = 0;
            srconv_record.src_ratio = (float)samplerate_out/cfg.audio.samplerate;
            srconv_record.input_frames = frameCount;
            srconv_record.output_frames = frameCount*cfg.audio.channel * (srconv_record.src_ratio+1) * sizeof(float);

            src_short_to_float_array((short*)pa_pcm_buf, srconv_record_in_buf, frameCount*cfg.audio.channel);

            //The actual resample process
            src_process(srconv_state_record, &srconv_record);

            src_float_to_short_array(srconv_record.data_out, snd_conv_work_buf,
                                     srconv_record.output_frames_gen*cfg.audio.channel);

            rb_write(&rec_rb, (char*)snd_conv_work_buf,
                     srconv_record.output_frames_gen*sizeof(short)*cfg.audio.channel);

        }
        else
            rb_write(&rec_rb, (char*)pa_pcm_buf, frameCount*sizeof(short)*cfg.audio.channel);

		pthread_cond_signal(&rec_cond);
	}
    
    //tell vu_update() that there is new audio data
    pa_new_frames = 1;

    return 0;
}

void snd_update_vu(void)
{
    int i;
    int lpeak = 0;
    int rpeak = 0;
    short *p;

    if (!pa_pcm_buf || framepacket_size <= 0)
        return;

    p = pa_pcm_buf;
    for(i = 0; i < framepacket_size; i += cfg.audio.channel)
    {
        if(abs(p[i]) > lpeak)
            lpeak = abs(p[i]);
        if(abs(p[i+(cfg.audio.channel-1)]) > rpeak)
            rpeak = abs(p[i+(cfg.audio.channel-1)]);
    }

    vu_meter(lpeak, rpeak);

    pa_new_frames = 0;
}

snd_dev_t **snd_get_devices(int *dev_count)
{
	int i, j;
    int devcount, sr_count, dev_num;
    bool sr_supported = 0;
    const PaDeviceInfo *p_di;
    char info_buf[256];
    PaStreamParameters pa_params;

    int sr[] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };

    snd_dev_t **dev_list;

	dev_num = 0;

    dev_list = (snd_dev_t**)malloc(100*sizeof(snd_dev_t*));

	//100 sound devices should be enough
    for(i = 0; i < 100; i++)
        dev_list[i] = (snd_dev_t*)malloc(sizeof(snd_dev_t));

    dev_list[dev_num]->name = (char*) malloc(strlen("Default PCM device (default)")+1);
    strcpy(dev_list[dev_num]->name, "Default PCM device (default)");
    dev_list[dev_num]->dev_id = Pa_GetDefaultInputDevice();
    dev_num++;


    devcount = Pa_GetDeviceCount();
    if(devcount < 0)
    {
        snprintf(info_buf, sizeof(info_buf), "PaError: %s", Pa_GetErrorText(devcount));
        print_info(info_buf, 1);
    }

    for(i = 0; i < devcount && i < 100; i++)
    {
        sr_count = 0;
        sr_supported = 0;
        p_di = Pa_GetDeviceInfo(i);
        if(p_di == NULL)
        {
            snprintf(info_buf, sizeof(info_buf), "Error getting device Info (%d)", i);
            print_info(info_buf, 1);
            continue;
        }


        //Save only devices which have input Channels
        if(p_di->maxInputChannels <= 0)
            continue;

        pa_params.device = i;
        {
            int dev_ch = cfg.audio.channel;
            if ((int)p_di->maxInputChannels < dev_ch)
                dev_ch = p_di->maxInputChannels;
            if (dev_ch < 1)
                dev_ch = 1;
            pa_params.channelCount = dev_ch;
        }
        pa_params.sampleFormat = paInt16;
        pa_params.suggestedLatency = p_di->defaultHighInputLatency;
        pa_params.hostApiSpecificStreamInfo = NULL;

        //add the supported samplerates to the device structure
        for(j = 0; j < 9; j++)
        {
            if(Pa_IsFormatSupported(&pa_params, NULL, sr[j]) != paInvalidSampleRate)
            {
                dev_list[dev_num]->sr_list[sr_count] = sr[j];
                sr_count++;
                sr_supported = 1;
            }
        }
        //Go to the next device if this one doesn't support at least one of our samplerates
        if(!sr_supported)
            continue;
        
        dev_list[dev_num]->num_of_sr = sr_count;

        //Mark the end of the samplerate list for this device with a 0
        dev_list[dev_num]->sr_list[sr_count] = 0;

        dev_list[dev_num]->name = (char*) malloc(strlen(p_di->name)+1);
        strcpy(dev_list[dev_num]->name, p_di->name);
        dev_list[dev_num]->dev_id = i;

        //copy the sr_list from the device where the
        //virtual default device points to
        if(dev_list[0]->dev_id == dev_list[dev_num]->dev_id)
        {
            memcpy(dev_list[0]->sr_list, dev_list[dev_num]->sr_list,
                    sizeof(dev_list[dev_num]->sr_list));
            
            dev_list[0]->num_of_sr = sr_count;
        }


        //We need to escape every '/' in the device name
        //otherwise FLTK will add a submenu for every '/' in the dev list
        strrpl(&dev_list[dev_num]->name, (char*)"/", (char*)"\\/", MODE_ALL);

        dev_num++;
    }//for(i = 0; i < devcount && i < 100; i++)


    if(dev_num == 1)
        *dev_count = 0;
    else
        *dev_count = dev_num;

    return dev_list;
}

snd_dev_t **snd_get_output_devices(int *dev_count)
{
    int i;
    int devcount, dev_num;
    const PaDeviceInfo *p_di;
    char info_buf[256];
    PaStreamParameters pa_params;
    snd_dev_t **dev_list;

    dev_num = 0;
    dev_list = (snd_dev_t**)malloc(100 * sizeof(snd_dev_t*));

    for (i = 0; i < 100; i++)
        dev_list[i] = (snd_dev_t*)malloc(sizeof(snd_dev_t));

    dev_list[dev_num]->name = (char*)malloc(strlen("Off (no monitor playback)") + 1);
    strcpy(dev_list[dev_num]->name, "Off (no monitor playback)");
    dev_list[dev_num]->dev_id = TTNS_MONITOR_OFF;
    dev_list[dev_num]->num_of_sr = 0;
    dev_list[dev_num]->sr_list[0] = 0;
    dev_num++;

    dev_list[dev_num]->name = (char*)malloc(strlen("Default output (default)") + 1);
    strcpy(dev_list[dev_num]->name, "Default output (default)");
    dev_list[dev_num]->dev_id = Pa_GetDefaultOutputDevice();
    dev_list[dev_num]->num_of_sr = 0;
    dev_list[dev_num]->sr_list[0] = 0;
    dev_num++;

    devcount = Pa_GetDeviceCount();
    if (devcount < 0)
    {
        snprintf(info_buf, sizeof(info_buf), "PaError: %s", Pa_GetErrorText(devcount));
        print_info(info_buf, 1);
    }

    for (i = 0; i < devcount && i < 100; i++)
    {
        p_di = Pa_GetDeviceInfo(i);
        if (p_di == NULL)
            continue;

        if (p_di->maxOutputChannels < 1)
            continue;

        pa_params.device = i;
        pa_params.channelCount = (p_di->maxOutputChannels > 1) ? 2 : 1;
        pa_params.sampleFormat = paInt16;
        pa_params.suggestedLatency = p_di->defaultLowOutputLatency;
        pa_params.hostApiSpecificStreamInfo = NULL;

        if (Pa_IsFormatSupported(NULL, &pa_params, cfg.audio.samplerate) != paFormatIsSupported)
            continue;

        dev_list[dev_num]->num_of_sr = 0;
        dev_list[dev_num]->sr_list[0] = 0;
        dev_list[dev_num]->name = (char*)malloc(strlen(p_di->name) + 1);
        strcpy(dev_list[dev_num]->name, p_di->name);
        dev_list[dev_num]->dev_id = i;
        strrpl(&dev_list[dev_num]->name, (char*)"/", (char*)"\\/", MODE_ALL);
        dev_num++;
    }

    if (dev_num == 1)
        *dev_count = 0;
    else
        *dev_count = dev_num;

    return dev_list;
}

void snd_reset_samplerate_conv(int rec_or_stream)
{
    int error;
    
    if (rec_or_stream == SND_STREAM)
    { 
        if (srconv_state_stream != NULL)
        {
            src_delete(srconv_state_stream);
            srconv_state_stream = NULL;
        }

        srconv_state_stream = src_new(cfg.audio.resample_mode, cfg.audio.channel, &error);
        if (srconv_state_stream == NULL)
        {
            print_info("ERROR: Could not initialize samplerate converter", 0);
        }
    }

    if (rec_or_stream == SND_REC)
    { 
        if (srconv_state_record != NULL)
        {
            src_delete(srconv_state_record);
            srconv_state_record = NULL;
        }


        srconv_state_record = src_new(cfg.audio.resample_mode, cfg.audio.channel, &error);
        if (srconv_state_record == NULL)
        {
            print_info("ERROR: Could not initialize samplerate converter", 0);
        }
    }
}

void snd_close(void)
{
    snd_audio_active = 0;

    if (mic_stream != NULL)
    {
        Pa_StopStream(mic_stream);
        Pa_CloseStream(mic_stream);
        mic_stream = NULL;
    }

    if (stream != NULL)
    {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = NULL;
    }

    snd_close_monitor();

    Pa_Terminate();

    free(srconv_stream_in_buf);
    free(srconv_stream_out_buf);
    srconv_stream_in_buf = NULL;
    srconv_stream_out_buf = NULL;

    free(srconv_record_in_buf);
    free(srconv_record_out_buf);
    srconv_record_in_buf = NULL;
    srconv_record_out_buf = NULL;

    ttns_free_mix_buffers();
    ttns_mic_rb_shutdown();
    rb_free(&rec_rb);
    rb_free(&stream_rb);
    free(snd_conv_work_buf);
    snd_conv_work_buf = NULL;

    ttns_use_dual_mic = 0;
    ttns_use_shared_input = 0;
    mic_capture_peak = 0;

    free(pa_pcm_buf);
    pa_pcm_buf = NULL;
    free(encode_buf);
    encode_buf = NULL;
}


