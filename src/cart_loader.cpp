#include "cart_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <samplerate.h>

#include <lame/lame.h>
#include <FLAC/stream_decoder.h>
#include <vorbis/vorbisfile.h>

#include "wav_header.h"

typedef struct
{
    short *data;
    int frames;
    int cap;
} pcm_buf_t;

static int pcm_buf_grow(pcm_buf_t *buf, int need_frames)
{
    int new_cap;

    if (buf->frames + need_frames <= buf->cap)
        return 0;

    new_cap = buf->cap ? buf->cap : 4096;
    while (buf->frames + need_frames > new_cap)
        new_cap *= 2;

    buf->data = (short*)realloc(buf->data, (size_t)new_cap * 2 * sizeof(short));
    if (!buf->data)
        return -1;

    buf->cap = new_cap;
    return 0;
}

static int pcm_buf_append_lr(pcm_buf_t *buf, const short *l, const short *r, int frames)
{
    int i;

    if (pcm_buf_grow(buf, frames) != 0)
        return -1;

    for (i = 0; i < frames; i++)
    {
        buf->data[(buf->frames + i) * 2] = l[i];
        buf->data[(buf->frames + i) * 2 + 1] = r[i];
    }
    buf->frames += frames;
    return 0;
}

static int pcm_buf_append_interleaved(pcm_buf_t *buf, const short *pcm, int channels, int frames)
{
    int i;

    if (pcm_buf_grow(buf, frames) != 0)
        return -1;

    if (channels == 1)
    {
        for (i = 0; i < frames; i++)
        {
            buf->data[(buf->frames + i) * 2] = pcm[i];
            buf->data[(buf->frames + i) * 2 + 1] = pcm[i];
        }
    }
    else
    {
        for (i = 0; i < frames; i++)
        {
            buf->data[(buf->frames + i) * 2] = pcm[i * 2];
            buf->data[(buf->frames + i) * 2 + 1] = pcm[i * 2 + 1];
        }
    }
    buf->frames += frames;
    return 0;
}

static void pcm_buf_free(pcm_buf_t *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->frames = 0;
    buf->cap = 0;
}

static short *resample_pcm(const short *in, int in_frames, int in_ch,
                           int in_sr, int out_sr, int *out_frames)
{
    SRC_DATA data;
    SRC_STATE *state;
    float *fin;
    float *fout;
    short *out;
    int err;
    int in_samples = in_frames * in_ch;
    int out_cap;
    int ratio;

    if (in_sr == out_sr)
    {
        out = (short*)malloc((size_t)in_samples * sizeof(short));
        if (!out)
            return NULL;
        memcpy(out, in, (size_t)in_samples * sizeof(short));
        *out_frames = in_frames;
        return out;
    }

    fin = (float*)malloc((size_t)in_samples * sizeof(float));
    ratio = (out_sr + in_sr - 1) / in_sr + 1;
    out_cap = in_frames * ratio + 64;
    fout = (float*)malloc((size_t)out_cap * in_ch * sizeof(float));
    if (!fin || !fout)
    {
        free(fin);
        free(fout);
        return NULL;
    }

    src_short_to_float_array(in, fin, in_samples);
    memset(&data, 0, sizeof(data));
    data.data_in = fin;
    data.data_out = fout;
    data.end_of_input = 1;
    data.input_frames = in_frames;
    data.output_frames = out_cap;
    data.src_ratio = (double)out_sr / (double)in_sr;

    state = src_new(SRC_SINC_MEDIUM_QUALITY, in_ch, &err);
    if (!state)
    {
        free(fin);
        free(fout);
        return NULL;
    }

    src_process(state, &data);
    src_delete(state);

    *out_frames = data.output_frames_gen;
    out = (short*)malloc((size_t)data.output_frames_gen * in_ch * sizeof(short));
    if (out)
        src_float_to_short_array(fout, out, data.output_frames_gen * in_ch);

    free(fin);
    free(fout);
    return out;
}

static short *finalize_pcm(pcm_buf_t *buf, int source_sr, int target_sr, int *out_frames)
{
    short *pcm;
    short *rs;

    if (!buf->data || buf->frames <= 0)
        return NULL;

    if (source_sr == target_sr)
    {
        pcm = buf->data;
        *out_frames = buf->frames;
        buf->data = NULL;
        buf->frames = 0;
        buf->cap = 0;
        return pcm;
    }

    rs = resample_pcm(buf->data, buf->frames, 2, source_sr, target_sr, out_frames);
    pcm_buf_free(buf);
    return rs;
}

short *cart_resample_stereo_pcm(const short *in, int in_frames, int in_sr, int target_sr,
                                int *out_frames)
{
    short *out;
    int frames;

    if (!in || in_frames <= 0 || !out_frames)
        return NULL;

    if (in_sr == target_sr)
    {
        size_t samples = (size_t)in_frames * 2;

        out = (short*)malloc(samples * sizeof(short));
        if (!out)
            return NULL;
        memcpy(out, in, samples * sizeof(short));
        *out_frames = in_frames;
        return out;
    }

    out = resample_pcm(in, in_frames, 2, in_sr, target_sr, &frames);
    if (!out)
    {
        *out_frames = 0;
        return NULL;
    }

    *out_frames = frames;
    return out;
}

static short *load_wav_stereo(const char *path, int *out_frames, int target_sr)
{
    FILE *fd;
    wav_hdr hdr;
    short *raw = NULL;
    short *pcm = NULL;
    pcm_buf_t buf;
    int frames;
    int ch;
    int i;
    size_t nread;
    int source_sr;

    memset(&buf, 0, sizeof(buf));

    fd = fopen(path, "rb");
    if (!fd)
        return NULL;

    if (fread(hdr.data, 1, 44, fd) != 44)
    {
        fclose(fd);
        return NULL;
    }

    if (strncmp(hdr.wav.riff_id, "RIFF", 4) != 0 ||
        strncmp(hdr.wav.fmt_id, "fmt ", 4) != 0 ||
        hdr.wav.fmt_format != 1 ||
        hdr.wav.fmt_bps != 16)
    {
        fclose(fd);
        return NULL;
    }

    ch = hdr.wav.fmt_channel;
    if (ch < 1 || ch > 2)
    {
        fclose(fd);
        return NULL;
    }

    source_sr = hdr.wav.fmt_samplerate;
    frames = hdr.wav.data_size / (ch * 2);
    raw = (short*)malloc((size_t)frames * ch * sizeof(short));
    if (!raw)
    {
        fclose(fd);
        return NULL;
    }

    nread = fread(raw, sizeof(short), (size_t)frames * ch, fd);
    fclose(fd);
    if ((int)nread != frames * ch)
    {
        free(raw);
        return NULL;
    }

    if (ch == 1)
    {
        pcm = (short*)malloc((size_t)frames * 2 * sizeof(short));
        if (!pcm)
        {
            free(raw);
            return NULL;
        }
        for (i = 0; i < frames; i++)
        {
            pcm[i * 2] = raw[i];
            pcm[i * 2 + 1] = raw[i];
        }
        free(raw);
    }
    else
    {
        pcm = raw;
    }

    buf.data = pcm;
    buf.frames = frames;
    buf.cap = frames;
    pcm = finalize_pcm(&buf, source_sr, target_sr, out_frames);
    return pcm;
}

static short *load_mp3_stereo(const char *path, int *out_frames, int target_sr)
{
    FILE *fd;
    hip_t hip;
    unsigned char mp3buf[16384];
    short pcm_l[1152];
    short pcm_r[1152];
    pcm_buf_t buf;
    mp3data_struct mp3data;
    size_t len;
    int iret;
    int source_sr = 44100;
    int eof = 0;

    memset(&buf, 0, sizeof(buf));
    memset(&mp3data, 0, sizeof(mp3data));

    fd = fopen(path, "rb");
    if (!fd)
        return NULL;

    hip = hip_decode_init();
    if (!hip)
    {
        fclose(fd);
        return NULL;
    }

    while (!eof)
    {
        len = fread(mp3buf, 1, sizeof(mp3buf), fd);
        if (len == 0)
            eof = 1;

        if (len > 0)
        {
            iret = hip_decode_headers(hip, mp3buf, len, pcm_l, pcm_r, &mp3data);
            if (iret < 0)
                break;
            if (mp3data.header_parsed && mp3data.samplerate > 0)
                source_sr = mp3data.samplerate;
            if (iret > 0 && pcm_buf_append_lr(&buf, pcm_l, pcm_r, iret) != 0)
                break;
        }

        for (;;)
        {
            iret = hip_decode(hip, mp3buf, 0, pcm_l, pcm_r);
            if (iret < 0)
                break;
            if (iret == 0)
                break;
            if (pcm_buf_append_lr(&buf, pcm_l, pcm_r, iret) != 0)
            {
                iret = -1;
                break;
            }
        }

        if (iret < 0)
            break;
    }

    hip_decode_exit(hip);
    fclose(fd);

    if (iret < 0 || !buf.data || buf.frames <= 0)
    {
        pcm_buf_free(&buf);
        return NULL;
    }

    return finalize_pcm(&buf, source_sr, target_sr, out_frames);
}

typedef struct
{
    pcm_buf_t *buf;
    unsigned bits_per_sample;
} flac_load_ctx_t;

static FLAC__StreamDecoderWriteStatus flac_write_cb(
    const FLAC__StreamDecoder *,
    const FLAC__Frame *frame,
    const FLAC__int32 * const buffer[],
    void *client_data)
{
    flac_load_ctx_t *ctx = (flac_load_ctx_t*)client_data;
    unsigned ch = frame->header.channels;
    unsigned bps = frame->header.bits_per_sample;
    unsigned n = frame->header.blocksize;
    unsigned i;
    short l, r;
    int shift;

    ctx->bits_per_sample = bps;
    shift = (bps > 16) ? (int)(bps - 16) : 0;

    for (i = 0; i < n; i++)
    {
        if (ch == 1)
        {
            l = (short)(buffer[0][i] >> shift);
            r = l;
        }
        else
        {
            l = (short)(buffer[0][i] >> shift);
            r = (short)(buffer[1][i] >> shift);
        }

        if (pcm_buf_append_lr(ctx->buf, &l, &r, 1) != 0)
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static short *load_flac_stereo(const char *path, int *out_frames, int target_sr)
{
    FLAC__StreamDecoder *decoder;
    flac_load_ctx_t ctx;
    FLAC__StreamDecoderInitStatus status;
    unsigned source_sr = 44100;

    memset(&ctx, 0, sizeof(ctx));
    ctx.buf = (pcm_buf_t*)calloc(1, sizeof(pcm_buf_t));
    if (!ctx.buf)
        return NULL;

    decoder = FLAC__stream_decoder_new();
    if (!decoder)
    {
        free(ctx.buf);
        return NULL;
    }

    status = FLAC__stream_decoder_init_file(decoder, path, flac_write_cb, NULL, NULL, &ctx);
    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        FLAC__stream_decoder_delete(decoder);
        free(ctx.buf);
        return NULL;
    }

    if (!FLAC__stream_decoder_process_until_end_of_stream(decoder))
    {
        FLAC__stream_decoder_finish(decoder);
        FLAC__stream_decoder_delete(decoder);
        pcm_buf_free(ctx.buf);
        free(ctx.buf);
        return NULL;
    }

    source_sr = FLAC__stream_decoder_get_sample_rate(decoder);
    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);

    {
        short *pcm = finalize_pcm(ctx.buf, (int)source_sr, target_sr, out_frames);
        free(ctx.buf);
        return pcm;
    }
}

static short *load_ogg_stereo(const char *path, int *out_frames, int target_sr)
{
    FILE *fd;
    OggVorbis_File vf;
    vorbis_info *vi;
    char chunk[4096];
    pcm_buf_t buf;
    long ret;
    int current_section;
    int source_sr;

    memset(&buf, 0, sizeof(buf));

    fd = fopen(path, "rb");
    if (!fd)
        return NULL;

    if (ov_open(fd, &vf, NULL, 0) < 0)
    {
        fclose(fd);
        return NULL;
    }

    vi = ov_info(&vf, -1);
    if (!vi || vi->channels < 1)
    {
        ov_clear(&vf);
        return NULL;
    }

    source_sr = vi->rate;

    while (1)
    {
        ret = ov_read(&vf, chunk, sizeof(chunk), 0, 2, 1, &current_section);
        if (ret == 0)
            break;
        if (ret < 0)
        {
            ov_clear(&vf);
            pcm_buf_free(&buf);
            return NULL;
        }

        if (pcm_buf_append_interleaved(&buf, (const short*)chunk, vi->channels, (int)(ret / 2 / vi->channels)) != 0)
        {
            ov_clear(&vf);
            pcm_buf_free(&buf);
            return NULL;
        }
    }

    ov_clear(&vf);
    return finalize_pcm(&buf, source_sr, target_sr, out_frames);
}

static int path_ext_is(const char *path, const char *ext)
{
    const char *dot = strrchr(path, '.');
    size_t elen;
    size_t i;

    if (!dot)
        return 0;

    elen = strlen(ext);
    if (strlen(dot + 1) != elen)
        return 0;

    for (i = 0; i < elen; i++)
    {
        if (tolower((unsigned char)dot[1 + i]) != tolower((unsigned char)ext[i]))
            return 0;
    }
    return 1;
}

static int sniff_format(const char *path)
{
    FILE *fd;
    unsigned char hdr[12];
    size_t n;

    fd = fopen(path, "rb");
    if (!fd)
        return -1;

    n = fread(hdr, 1, sizeof(hdr), fd);
    fclose(fd);
    if (n < 4)
        return -1;

    if (n >= 12 && !memcmp(hdr, "RIFF", 4) && !memcmp(hdr + 8, "WAVE", 4))
        return 'w';
    if (!memcmp(hdr, "fLaC", 4))
        return 'f';
    if (!memcmp(hdr, "OggS", 4))
        return 'o';
    if (!memcmp(hdr, "ID3", 3))
        return 'm';
    if (hdr[0] == 0xff && (hdr[1] & 0xe0) == 0xe0)
        return 'm';
    if (n >= 12 && !memcmp(hdr + 4, "ftyp", 4))
        return 'a';

    if (path_ext_is(path, "wav"))
        return 'w';
    if (path_ext_is(path, "mp3"))
        return 'm';
    if (path_ext_is(path, "m4a") || path_ext_is(path, "aac"))
        return 'a';
    if (path_ext_is(path, "flac"))
        return 'f';
    if (path_ext_is(path, "ogg"))
        return 'o';

    return -1;
}

short *cart_load_stereo_pcm(const char *path, int target_sr, int *out_frames)
{
    int fmt;
    short *pcm;

    if (!path || !path[0] || !out_frames)
        return NULL;

    *out_frames = 0;
    fmt = sniff_format(path);

    switch (fmt)
    {
    case 'w':
        pcm = load_wav_stereo(path, out_frames, target_sr);
        break;
    case 'm':
#ifdef __APPLE__
        pcm = cart_load_av_stereo(path, target_sr, out_frames);
#else
        pcm = load_mp3_stereo(path, out_frames, target_sr);
#endif
        break;
    case 'a':
#ifdef __APPLE__
        pcm = cart_load_av_stereo(path, target_sr, out_frames);
#else
        pcm = NULL;
#endif
        break;
    case 'f':
        pcm = load_flac_stereo(path, out_frames, target_sr);
        break;
    case 'o':
        pcm = load_ogg_stereo(path, out_frames, target_sr);
        break;
    default:
        return NULL;
    }

    if (!pcm || *out_frames <= 0)
    {
        free(pcm);
        *out_frames = 0;
        return NULL;
    }

    return pcm;
}
