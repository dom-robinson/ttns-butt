#ifndef CART_LOADER_H
#define CART_LOADER_H

/* Decode WAV / MP3 / M4A (macOS) / FLAC / OGG to stereo int16 PCM at target_sr. Caller frees. */
short *cart_load_stereo_pcm(const char *path, int target_sr, int *out_frames);

/* Resample stereo int16 PCM. Caller frees. */
short *cart_resample_stereo_pcm(const short *in, int in_frames, int in_sr, int target_sr,
                                int *out_frames);

#ifdef __APPLE__
short *cart_load_av_stereo(const char *path, int target_sr, int *out_frames);
#endif

#endif
