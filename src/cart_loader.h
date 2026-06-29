#ifndef CART_LOADER_H
#define CART_LOADER_H

/* Decode WAV / MP3 / FLAC / OGG to stereo int16 PCM at target_sr. Caller frees. */
short *cart_load_stereo_pcm(const char *path, int target_sr, int *out_frames);

#endif
