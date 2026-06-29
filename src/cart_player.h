#ifndef CART_PLAYER_H
#define CART_PLAYER_H

#define TTNS_CART_SLOTS 8
#define TTNS_CART_FADE_MS 300

enum {
    TTNS_CART_ONESHOT = 0,
    TTNS_CART_LOOP = 1
};

void ttns_cart_init(int samplerate);
void ttns_cart_shutdown(void);
int ttns_cart_load(int slot, const char *path);
void ttns_cart_clear(int slot);
void ttns_cart_set_mode(int slot, int mode);
void ttns_cart_set_gain(int slot, float gain);
float ttns_cart_get_gain(int slot);
void ttns_cart_set_label(int slot, const char *label);
const char *ttns_cart_get_label(int slot);
void ttns_cart_trigger(int slot);
void ttns_cart_render(short *out_stereo, int frames);
int ttns_cart_any_playing(void);
int ttns_cart_is_playing(int slot);
int ttns_cart_has_audio(int slot);
int ttns_cart_get_mode(int slot);

#endif
