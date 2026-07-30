/*
 * TTNS Remote — co-host client (LAN discovery + Opus/TCP duplex).
 * Themed to match TTNS Deck; opens AirPods as a duplex stream when possible.
 */

#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include "FL/Fl_Ttns_Check_Button.H"
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include <portaudio.h>
#include <opus/opus.h>
#include <samplerate.h>

#include <limits.h>

#include "config.h"
#include "ringbuffer.h"
#include "ttns_paths.h"
#include "ttns_remote_net.h"
#include "ttns_remote_proto.h"
#include "ttns_remote_wan.h"

#ifdef _WIN32
#include <winsock2.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <arpa/inet.h>
#endif

/* Deck palette */
static Fl_Color col_bg(void)    { return fl_rgb_color(0, 0, 0); }
static Fl_Color col_red(void)   { return fl_rgb_color(227, 27, 35); }
static Fl_Color col_green(void) { return fl_rgb_color(51, 255, 51); }
static Fl_Color col_dark(void)  { return fl_rgb_color(24, 24, 24); }
static Fl_Color col_orange(void){ return fl_rgb_color(255, 153, 0); }
static Fl_Color col_meter_red(void){ return fl_rgb_color(255, 48, 48); }

/* Same VU mapping as Deck Fl_Ttns_Fader / vu_meter (VU_OFFSET = +6 dB).
 * Maps roughly −50…+6 dBFS → 0…1 so quiet speech lights the bar. */
#define REMOTE_VU_OFFSET 6.0f

static float float_peak_to_level(float peak)
{
    float db;

    if (peak <= 1e-6f)
        return 0.0f;
    if (peak > 1.0f)
        peak = 1.0f;

    db = 20.0f * (float)log10((double)peak) + REMOTE_VU_OFFSET;
    if (db < -50.0f)
        db = -50.0f;
    if (db > 6.0f)
        db = 6.0f;
    return (db + 50.0f) / 56.0f;
}

class TtnsMeter : public Fl_Widget {
    float level;
public:
    TtnsMeter(int X, int Y, int W, int H, const char *L = 0)
        : Fl_Widget(X, Y, W, H, L), level(0.f) {}
    void set_level(float v)
    {
        if (v < 0.f) v = 0.f;
        if (v > 1.f) v = 1.f;
        /* Smooth like Deck faders so quiet levels still climb visibly. */
        level = level * 0.55f + v * 0.45f;
        if (level < 0.002f)
            level = 0.f;
        redraw();
    }
    void draw()
    {
        int i, n = 24;
        int gap = 1;
        int bar_w = (w() - (n - 1) * gap) / n;
        if (bar_w < 2) bar_w = 2;
        fl_color(col_dark());
        fl_rectf(x(), y(), w(), h());
        fl_color(col_red());
        fl_rect(x(), y(), w(), h());
        for (i = 0; i < n; i++)
        {
            float thr = (float)(i + 1) / (float)n;
            int lit = level >= thr - 0.001f;
            Fl_Color c = col_green();
            if (i >= n - 3) c = col_meter_red();
            else if (i >= n - 8) c = col_orange();
            fl_color(lit ? c : fl_color_average(c, col_bg(), 0.25f));
            fl_rectf(x() + 2 + i * (bar_w + gap), y() + 2, bar_w, h() - 4);
        }
    }
};

static Fl_Input *room_input = NULL;
static Fl_Box *status_box = NULL;
static Fl_Box *conn_banner = NULL;
static Fl_Box *core_phone = NULL;
static Fl_Button *link_btn = NULL;
static Fl_Ttns_Check_Button *mic_mute = NULL;
static Fl_Ttns_Check_Button *ptt_mode = NULL;
static Fl_Slider *mic_level = NULL;
static Fl_Slider *hp_level = NULL;
static Fl_Choice *mic_choice = NULL;
static Fl_Choice *hp_choice = NULL;
static TtnsMeter *mic_meter = NULL;
static TtnsMeter *hp_meter = NULL;
static Fl_Box *slot_box = NULL;
static char status_text[192] = "Ready — enter Deck room code, then Connect";
static char peer_host[64] = "";
static int ui_was_connected = -1;

static int sock_fd = -1;
static int via_wan = 0;
static int my_slot = -1;
static volatile int connected = 0;
static volatile int running = 0;
static pthread_t rx_thread;
static pthread_t tx_thread;
static OpusEncoder *enc = NULL;
static OpusDecoder *dec = NULL;
static PaStream *audio_stream = NULL;
static struct ringbuf cap_rb;
static struct ringbuf play_rb;
static int cap_rb_ok = 0;
static int play_rb_ok = 0;
static uint16_t tx_seq = 0;
static float mic_gain = 1.0f;
static float hp_gain = 1.0f;
static int mic_devs[128];
static int hp_devs[128];
static int mic_n = 0;
static int hp_n = 0;

static double device_rate = TTNS_REMOTE_SAMPLERATE;
static int device_frames = TTNS_REMOTE_FRAME_SAMPLES;
static int duplex_mode = 0;
static int in_channels = 1;
static int out_channels = 2;
static SRC_STATE *cap_src = NULL;
static SRC_STATE *play_src = NULL;
static float cap_src_ratio = 1.0f;
static float play_src_ratio = 1.0f;
static int play_preroll_done = 0;
static int play_underruns = 0;
static int play_is_bt = 0;
static float play_src_leftover[4096];
static int play_src_leftover_n = 0;

/* Wired: tight. Bluetooth: larger cushion — AirPods callbacks are bursty. */
#define PLAY_TARGET_FRAMES     (TTNS_REMOTE_FRAME_SAMPLES * 3)
#define PLAY_PREROLL_FRAMES    (TTNS_REMOTE_FRAME_SAMPLES * 2)
#define PLAY_MAX_FRAMES        (TTNS_REMOTE_FRAME_SAMPLES * 12)
#define PLAY_TARGET_FRAMES_BT  (TTNS_REMOTE_FRAME_SAMPLES * 8)   /* ~160 ms */
#define PLAY_PREROLL_FRAMES_BT (TTNS_REMOTE_FRAME_SAMPLES * 6)   /* ~120 ms */
#define PLAY_MAX_FRAMES_BT     (TTNS_REMOTE_FRAME_SAMPLES * 30)  /* ~600 ms */

static volatile float ui_mic_peak = 0.f;
static volatile float ui_hp_peak = 0.f;

static volatile int g_mic_mute = 0;
static volatile int g_ptt_mode = 0;
static volatile int g_ptt_held = 0;

typedef struct {
    char room_code[12];
    char mic_name[256];
    char hp_name[256];
    float mic_db;
    float hp_db;
} remote_prefs_t;

static remote_prefs_t g_prefs;

static int play_target_frames(void)
{
    return play_is_bt ? PLAY_TARGET_FRAMES_BT : PLAY_TARGET_FRAMES;
}

static int play_preroll_frames(void)
{
    return play_is_bt ? PLAY_PREROLL_FRAMES_BT : PLAY_PREROLL_FRAMES;
}

static int play_max_frames(void)
{
    return play_is_bt ? PLAY_MAX_FRAMES_BT : PLAY_MAX_FRAMES;
}

static int remote_prefs_path(char *out, size_t outlen)
{
    const char *home;

    if (!out || outlen < 8)
        return -1;
#ifdef _WIN32
    home = getenv("APPDATA");
    if (!home || !home[0])
        home = getenv("USERPROFILE");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s\\TTNS Remote\\prefs.txt", home);
#elif defined(__APPLE__)
    home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s/Library/Application Support/TTNS Remote/prefs.txt", home);
#else
    home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s/.config/ttns-remote/prefs.txt", home);
#endif
    return 0;
}

static void remote_prefs_ensure_dir(const char *prefs_path)
{
    char dir[PATH_MAX];
    char *sep;
    char *p;

    if (!prefs_path || !prefs_path[0])
        return;
    strncpy(dir, prefs_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
#ifdef _WIN32
    sep = strrchr(dir, '\\');
    if (!sep)
        sep = strrchr(dir, '/');
#else
    sep = strrchr(dir, '/');
#endif
    if (!sep)
        return;
    *sep = '\0';

    for (p = dir + 1; *p; p++)
    {
#ifdef _WIN32
        if (*p != '\\' && *p != '/')
            continue;
#else
        if (*p != '/')
            continue;
#endif
        *p = '\0';
#ifdef _WIN32
        _mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
        *p =
#ifdef _WIN32
            '\\'
#else
            '/'
#endif
            ;
    }
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void remote_prefs_defaults(remote_prefs_t *p)
{
    memset(p, 0, sizeof(*p));
    p->mic_db = 0.f;
    p->hp_db = 0.f;
}

static void remote_prefs_load(remote_prefs_t *p)
{
    char path[PATH_MAX];
    FILE *f;
    char line[512];

    remote_prefs_defaults(p);
    if (remote_prefs_path(path, sizeof(path)) != 0)
        return;
    f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f))
    {
        char *nl = strchr(line, '\n');
        char *cr = strchr(line, '\r');
        if (nl) *nl = '\0';
        if (cr) *cr = '\0';
        if (strncmp(line, "room_code=", 10) == 0)
            ttns_rnet_normalize_room(p->room_code, sizeof(p->room_code), line + 10);
        else if (strncmp(line, "mic_name=", 9) == 0)
            strncpy(p->mic_name, line + 9, sizeof(p->mic_name) - 1);
        else if (strncmp(line, "hp_name=", 8) == 0)
            strncpy(p->hp_name, line + 8, sizeof(p->hp_name) - 1);
        else if (strncmp(line, "mic_db=", 7) == 0)
            p->mic_db = (float)atof(line + 7);
        else if (strncmp(line, "hp_db=", 6) == 0)
            p->hp_db = (float)atof(line + 6);
    }
    fclose(f);
}

static void remote_prefs_save(const remote_prefs_t *p)
{
    char path[PATH_MAX];
    FILE *f;

    if (!p)
        return;
    if (remote_prefs_path(path, sizeof(path)) != 0)
        return;
    remote_prefs_ensure_dir(path);
    f = fopen(path, "w");
    if (!f)
        return;
    if (p->room_code[0])
        fprintf(f, "room_code=%s\n", p->room_code);
    if (p->mic_name[0])
        fprintf(f, "mic_name=%s\n", p->mic_name);
    if (p->hp_name[0])
        fprintf(f, "hp_name=%s\n", p->hp_name);
    fprintf(f, "mic_db=%.2f\n", p->mic_db);
    fprintf(f, "hp_db=%.2f\n", p->hp_db);
    fclose(f);
}

static void remote_prefs_capture_ui(void)
{
    if (room_input && room_input->value() && room_input->value()[0])
        ttns_rnet_normalize_room(g_prefs.room_code, sizeof(g_prefs.room_code),
                                 room_input->value());
    if (mic_choice && mic_choice->value() >= 0)
    {
        const char *n = mic_choice->text(mic_choice->value());
        if (n)
            strncpy(g_prefs.mic_name, n, sizeof(g_prefs.mic_name) - 1);
    }
    if (hp_choice && hp_choice->value() >= 0)
    {
        const char *n = hp_choice->text(hp_choice->value());
        if (n)
            strncpy(g_prefs.hp_name, n, sizeof(g_prefs.hp_name) - 1);
    }
    if (mic_level)
        g_prefs.mic_db = (float)mic_level->value();
    if (hp_level)
        g_prefs.hp_db = (float)hp_level->value();
}

static void remote_prefs_save_room(const char *code)
{
    ttns_rnet_normalize_room(g_prefs.room_code, sizeof(g_prefs.room_code), code);
    remote_prefs_capture_ui();
    remote_prefs_save(&g_prefs);
}

static int name_score_match(const char *want, const char *have)
{
    char a[256], b[256];
    size_t i;
    if (!want || !want[0] || !have || !have[0])
        return 0;
    if (strcmp(want, have) == 0)
        return 100;
    for (i = 0; i < sizeof(a) - 1 && want[i]; i++)
        a[i] = (char)tolower((unsigned char)want[i]);
    a[i] = '\0';
    for (i = 0; i < sizeof(b) - 1 && have[i]; i++)
        b[i] = (char)tolower((unsigned char)have[i]);
    b[i] = '\0';
    if (strcmp(a, b) == 0)
        return 90;
    if (strstr(b, a) || strstr(a, b))
        return 50;
    return 0;
}

static void style_choice(Fl_Choice *c)
{
    c->box(FL_BORDER_BOX);
    c->down_box(FL_BORDER_BOX);
    c->color(col_bg(), col_red());
    c->selection_color(col_dark());
    c->textcolor(col_red());
    c->labelcolor(col_green());
}

static void style_slider(Fl_Slider *s)
{
    s->type(FL_HOR_SLIDER);
    s->box(FL_FLAT_BOX);
    s->color(col_dark());
    s->selection_color(col_red());
    s->labelcolor(col_green());
}

static void style_btn(Fl_Button *b)
{
    b->box(FL_BORDER_BOX);
    b->color(col_bg());
    b->selection_color(col_dark());
    b->labelcolor(col_green());
}

static void style_check(Fl_Ttns_Check_Button *c)
{
    c->box(FL_NO_BOX);
    c->down_box(FL_NO_BOX);
    c->color(col_bg());
    c->selection_color(col_red());
    c->labelcolor(col_green());
}

static void set_status(const char *s)
{
    snprintf(status_text, sizeof(status_text), "%s", s);
    if (status_box)
    {
        status_box->copy_label(status_text);
        status_box->labelcolor(col_green());
        status_box->redraw();
    }
}

static void update_connection_ui(void)
{
    int on = connected ? 1 : 0;
    char banner[96];
    char slotl[12];

    if (link_btn)
    {
        if (on)
        {
            link_btn->copy_label("Disconnect");
            link_btn->labelcolor(col_red());
            link_btn->color(col_dark());
        }
        else
        {
            link_btn->copy_label("Connect");
            link_btn->labelcolor(col_green());
            link_btn->color(col_bg());
        }
        link_btn->activate();
        link_btn->redraw();
    }

    if (on)
        snprintf(slotl, sizeof(slotl), "R%d", my_slot + 1);
    else
        snprintf(slotl, sizeof(slotl), "R—");
    if (slot_box)
    {
        slot_box->copy_label(slotl);
        slot_box->labelcolor(on ? col_green() : fl_color_average(col_green(), col_bg(), 0.4f));
        slot_box->color(on ? fl_color_average(col_green(), col_bg(), 0.15f) : col_bg());
        slot_box->redraw();
    }

    if (core_phone)
    {
        Fl_Color phone_col;
        if (on)
            phone_col = col_green();
        else if (ttns_core_reach_get())
            phone_col = fl_rgb_color(255, 200, 40);
        else
            phone_col = fl_color_average(col_green(), col_bg(), 0.35f);
        if (core_phone->labelcolor() != phone_col)
        {
            core_phone->labelcolor(phone_col);
            core_phone->redraw();
        }
    }

    if (conn_banner)
    {
        if (on)
        {
            snprintf(banner, sizeof(banner),
                     "CONNECTED  ·  you are %s on the Deck%s%s",
                     slotl,
                     peer_host[0] ? " @ " : "",
                     peer_host[0] ? peer_host : "");
            conn_banner->copy_label(banner);
            conn_banner->color(fl_rgb_color(0, 48, 0));
            conn_banner->labelcolor(col_green());
            conn_banner->labelfont(FL_BOLD);
        }
        else
        {
            conn_banner->copy_label("NOT CONNECTED  ·  enter room code, then Connect");
            conn_banner->color(col_dark());
            conn_banner->labelcolor(col_orange());
            conn_banner->labelfont(FL_BOLD);
        }
        conn_banner->redraw();
    }

    /* Lock join settings while live so the remote doesn't change mid-call. */
    if (room_input)
    {
        if (on) room_input->deactivate();
        else room_input->activate();
    }
    if (mic_choice)
    {
        if (on) mic_choice->deactivate();
        else mic_choice->activate();
    }
    if (hp_choice)
    {
        if (on) hp_choice->deactivate();
        else hp_choice->activate();
    }

    ui_was_connected = on;
}

static void meter_tick(void *)
{
    float mic_lin = ui_mic_peak * mic_gain;
    float hp_lin = ui_hp_peak; /* already includes hp_gain in play path */

    if (mic_meter)
        mic_meter->set_level(float_peak_to_level(mic_lin));
    if (hp_meter)
        hp_meter->set_level(float_peak_to_level(hp_lin));
    /* Decay held peaks (~same ballpark as Deck peak-hold fade). */
    ui_mic_peak *= 0.92f;
    ui_hp_peak *= 0.92f;
    if (ui_mic_peak < 1e-5f)
        ui_mic_peak = 0.f;
    if (ui_hp_peak < 1e-5f)
        ui_hp_peak = 0.f;

    /* Sync UI if the network thread dropped the link. */
    if (ui_was_connected != (connected ? 1 : 0))
    {
        if (!connected)
        {
            peer_host[0] = '\0';
            set_status("Disconnected from Deck");
        }
        update_connection_ui();
    }
    else if (connected && g_ptt_mode && !g_ptt_held && status_box)
    {
        char buf[160];
        snprintf(buf, sizeof(buf), "CONNECTED as R%d — hold Ctrl to talk (PTT)", my_slot + 1);
        if (strcmp(status_text, buf) != 0)
            set_status(buf);
    }
    /* Refresh telephone LED when core reachability flips (background probe). */
    if (core_phone)
    {
        Fl_Color phone_col;
        if (connected)
            phone_col = col_green();
        else if (ttns_core_reach_get())
            phone_col = fl_rgb_color(255, 200, 40);
        else
            phone_col = fl_color_average(col_green(), col_bg(), 0.35f);
        if (core_phone->labelcolor() != phone_col)
        {
            core_phone->labelcolor(phone_col);
            core_phone->redraw();
        }
    }
    Fl::repeat_timeout(0.05, meter_tick);
}

static int mic_is_muted(void)
{
    if (g_ptt_mode)
        return !g_ptt_held;
    return g_mic_mute;
}

static void mute_opts_cb(Fl_Widget *, void *)
{
    g_mic_mute = mic_mute && mic_mute->value();
    g_ptt_mode = ptt_mode && ptt_mode->value();
}

static void ptt_tick(void *)
{
    g_ptt_held = Fl::event_state(FL_CTRL) ? 1 : 0;
    Fl::repeat_timeout(0.05, ptt_tick);
}

static float peak_of(const float *x, int n)
{
    float p = 0.f;
    int i;
    for (i = 0; i < n; i++)
    {
        float a = fabsf(x[i]);
        if (a > p) p = a;
    }
    return p;
}

static void push_cap_floats(const float *mono, int frames)
{
    float peak;
    if (!cap_rb_ok || frames <= 0)
        return;
    peak = peak_of(mono, frames);
    if (peak > ui_mic_peak)
        ui_mic_peak = peak;

    if (cap_src && fabs(cap_src_ratio - 1.0) > 0.001)
    {
        float out[4096];
        SRC_DATA d;
        int gen;
        memset(&d, 0, sizeof(d));
        d.data_in = mono;
        d.input_frames = frames;
        d.data_out = out;
        d.output_frames = (long)(sizeof(out) / sizeof(out[0]));
        d.src_ratio = cap_src_ratio;
        if (src_process(cap_src, &d) == 0 && d.output_frames_gen > 0)
        {
            gen = (int)d.output_frames_gen;
            rb_write_drop(&cap_rb, (char *)out, (unsigned)(gen * sizeof(float)));
        }
        return;
    }
    rb_write_drop(&cap_rb, (char *)mono, (unsigned)(frames * sizeof(float)));
}

static void fill_play_floats(float *stereo, int frames)
{
    int i;
    float mono_dev[4096];
    int filled_frames;
    int target = play_target_frames();
    int preroll = play_preroll_frames();
    int maxf = play_max_frames();
    int got = 0;

    memset(stereo, 0, (size_t)frames * (size_t)out_channels * sizeof(float));
    if (!play_rb_ok || frames <= 0)
        return;
    if (frames > 4096)
        frames = 4096;

    filled_frames = rb_filled(&play_rb) / (int)sizeof(float);
    if (filled_frames < 1 && play_src_leftover_n < 1)
        return;

    /* Drop backlog if TCP bursted ahead of Bluetooth. */
    if (filled_frames > maxf)
    {
        unsigned drop = (unsigned)(filled_frames - target) * sizeof(float);
        drop -= drop % sizeof(float);
        if (drop > 0)
            rb_discard(&play_rb, drop);
        filled_frames = rb_filled(&play_rb) / (int)sizeof(float);
    }

    if (!play_preroll_done)
    {
        if (filled_frames < preroll)
            return; /* silence while jitter buffer fills */
        play_preroll_done = 1;
        play_underruns = 0;
    }

    memset(mono_dev, 0, (size_t)frames * sizeof(float));

    /* Drain SRC leftover first (libsamplerate does not always emit exact counts). */
    if (play_src_leftover_n > 0)
    {
        int take = play_src_leftover_n;
        if (take > frames)
            take = frames;
        memcpy(mono_dev, play_src_leftover, (size_t)take * sizeof(float));
        got = take;
        if (take < play_src_leftover_n)
        {
            memmove(play_src_leftover, play_src_leftover + take,
                    (size_t)(play_src_leftover_n - take) * sizeof(float));
            play_src_leftover_n -= take;
        }
        else
            play_src_leftover_n = 0;
    }

    if (got < frames && play_src && fabs(play_src_ratio - 1.0) > 0.001)
    {
        int need_out = frames - got;
        int need48 = (int)((double)need_out / play_src_ratio + 0.5) + 8;
        float in48[4096];
        float out_tmp[4096];
        SRC_DATA d;
        unsigned bytes48;

        if (need48 < 1)
            need48 = 1;
        if (need48 > 4096)
            need48 = 4096;
        filled_frames = rb_filled(&play_rb) / (int)sizeof(float);
        if (filled_frames < need48)
            need48 = filled_frames;
        if (need48 > 0)
        {
            bytes48 = (unsigned)need48 * sizeof(float);
            rb_read_len(&play_rb, (char *)in48, bytes48);
            memset(&d, 0, sizeof(d));
            d.data_in = in48;
            d.input_frames = need48;
            d.data_out = out_tmp;
            d.output_frames = (long)(sizeof(out_tmp) / sizeof(out_tmp[0]));
            d.src_ratio = play_src_ratio;
            if (src_process(play_src, &d) == 0 && d.output_frames_gen > 0)
            {
                int gen = (int)d.output_frames_gen;
                int use = gen;
                if (use > need_out)
                    use = need_out;
                memcpy(mono_dev + got, out_tmp, (size_t)use * sizeof(float));
                got += use;
                if (gen > use)
                {
                    int left = gen - use;
                    if (left > (int)(sizeof(play_src_leftover) / sizeof(play_src_leftover[0])))
                        left = (int)(sizeof(play_src_leftover) / sizeof(play_src_leftover[0]));
                    memcpy(play_src_leftover, out_tmp + use, (size_t)left * sizeof(float));
                    play_src_leftover_n = left;
                }
            }
        }
    }
    else if (got < frames)
    {
        int need = frames - got;
        filled_frames = rb_filled(&play_rb) / (int)sizeof(float);
        if (filled_frames < need)
            need = filled_frames;
        if (need > 0)
        {
            rb_read_len(&play_rb, (char *)(mono_dev + got), (unsigned)need * sizeof(float));
            got += need;
        }
    }

    if (got < frames)
    {
        /* Soft underrun: keep playing what we have; only re-preroll after sustained empty. */
        play_underruns++;
        if (got < 1 && play_underruns > (play_is_bt ? 4 : 2))
        {
            play_preroll_done = 0;
            play_underruns = 0;
        }
    }
    else
        play_underruns = 0;

    for (i = 0; i < frames; i++)
        mono_dev[i] *= hp_gain;

    for (i = 0; i < frames; i++)
    {
        float s = mono_dev[i];
        if (out_channels >= 2)
        {
            stereo[i * 2] = s;
            stereo[i * 2 + 1] = s;
        }
        else
            stereo[i] = s;
    }

    {
        float p = 0.f;
        for (i = 0; i < frames; i++)
        {
            float a = fabsf(mono_dev[i]);
            if (a > p)
                p = a;
        }
        if (p > ui_hp_peak)
            ui_hp_peak = p;
    }
}

static int pa_audio_cb(const void *input, void *output, unsigned long frames,
                       const PaStreamCallbackTimeInfo *t, PaStreamCallbackFlags f,
                       void *user)
{
    float mono[4096];
    unsigned long i;
    (void)t;
    (void)f;
    (void)user;

    if (frames > 4096)
        frames = 4096;

    if (input && duplex_mode)
    {
        const float *in = (const float *)input;
        for (i = 0; i < frames; i++)
        {
            if (in_channels >= 2)
                mono[i] = 0.5f * (in[i * 2] + in[i * 2 + 1]);
            else
                mono[i] = in[i];
        }
        push_cap_floats(mono, (int)frames);
    }
    else if (input && !duplex_mode)
    {
        const float *in = (const float *)input;
        for (i = 0; i < frames; i++)
        {
            if (in_channels >= 2)
                mono[i] = 0.5f * (in[i * 2] + in[i * 2 + 1]);
            else
                mono[i] = in[i];
        }
        push_cap_floats(mono, (int)frames);
    }

    if (output)
        fill_play_floats((float *)output, (int)frames);

    return paContinue;
}

static void *client_rx(void *arg)
{
    ttns_remote_hdr_t hdr;
    unsigned char payload[TTNS_REMOTE_MAX_PACKET];
    short pcm[TTNS_REMOTE_FRAME_SAMPLES];
    float out[TTNS_REMOTE_FRAME_SAMPLES];
    int n;
    int i;
    (void)arg;

    while (running && connected)
    {
        if (via_wan)
        {
            unsigned char raw[sizeof(ttns_remote_hdr_t) + TTNS_REMOTE_MAX_PACKET];
            int rn = ttns_wan_client_recv(raw, sizeof(raw), 200);
            if (rn < 0)
                break;
            if (rn < (int)sizeof(ttns_remote_hdr_t))
                continue;
            memcpy(&hdr, raw, sizeof(hdr));
            n = (int)ntohs(hdr.len);
            if (n < 0)
                continue;
            if (n > (int)sizeof(payload))
                n = (int)sizeof(payload);
            if (rn < (int)sizeof(hdr) + n)
                continue;
            memcpy(payload, raw + sizeof(hdr), (size_t)n);
        }
        else
        {
            n = ttns_rnet_recv_packet(sock_fd, &hdr, payload, sizeof(payload), 200);
            if (n < 0)
                break;
            if (n == 0)
                continue;
        }
        if (hdr.type == TTNS_PKT_BYE)
            break;
        if (hdr.type == TTNS_PKT_AUDIO && dec)
        {
            int frames = opus_decode(dec, payload, n, pcm, TTNS_REMOTE_FRAME_SAMPLES, 0);
            if (frames > 0 && play_rb_ok)
            {
                for (i = 0; i < frames; i++)
                    out[i] = ((float)pcm[i] / 32768.0f);
                rb_write_drop(&play_rb, (char *)out, (unsigned)(frames * sizeof(float)));
            }
        }
    }
    connected = 0;
    set_status("Disconnected");
    return NULL;
}

static void *client_tx(void *arg)
{
    float in[TTNS_REMOTE_FRAME_SAMPLES];
    short pcm[TTNS_REMOTE_FRAME_SAMPLES];
    unsigned char opus_buf[TTNS_REMOTE_MAX_PACKET];
    unsigned need = (unsigned)(TTNS_REMOTE_FRAME_SAMPLES * sizeof(float));
    (void)arg;

    while (running && connected)
    {
        int i;
        int nbytes;
        int filled;

        if (!cap_rb_ok)
        {
            struct timespec ts = {0, 5 * 1000 * 1000};
            nanosleep(&ts, NULL);
            continue;
        }

        filled = rb_filled(&cap_rb);
        if ((unsigned)filled < need)
        {
            struct timespec ts = {0, 2 * 1000 * 1000};
            nanosleep(&ts, NULL);
            continue;
        }
        rb_read_len(&cap_rb, (char *)in, need);

        for (i = 0; i < TTNS_REMOTE_FRAME_SAMPLES; i++)
        {
            float s = in[i] * mic_gain;
            if (mic_is_muted())
                s = 0.0f;
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            pcm[i] = (short)(s * 32767.0f);
        }

        nbytes = opus_encode(enc, pcm, TTNS_REMOTE_FRAME_SAMPLES, opus_buf, sizeof(opus_buf));
        if (nbytes > 0)
        {
            if (via_wan)
            {
                unsigned char pkt[sizeof(ttns_remote_hdr_t) + TTNS_REMOTE_MAX_PACKET];
                int total = ttns_rnet_build_packet(pkt, sizeof(pkt), TTNS_PKT_AUDIO,
                                                   (uint8_t)my_slot, tx_seq++, opus_buf,
                                                   (uint16_t)nbytes);
                if (total < 0 || ttns_wan_client_send(pkt, (size_t)total) != 0)
                    break;
            }
            else if (ttns_rnet_send_packet(sock_fd, TTNS_PKT_AUDIO, (uint8_t)my_slot,
                                           tx_seq++, opus_buf, (uint16_t)nbytes) != 0)
                break;
        }
    }
    connected = 0;
    return NULL;
}

static void destroy_src(void)
{
    if (cap_src)
    {
        src_delete(cap_src);
        cap_src = NULL;
    }
    if (play_src)
    {
        src_delete(play_src);
        play_src = NULL;
    }
}

static void stop_audio(void)
{
    if (audio_stream)
    {
        Pa_StopStream(audio_stream);
        Pa_CloseStream(audio_stream);
        audio_stream = NULL;
    }
    destroy_src();
    if (cap_rb_ok)
    {
        rb_free(&cap_rb);
        cap_rb_ok = 0;
    }
    if (play_rb_ok)
    {
        rb_free(&play_rb);
        play_rb_ok = 0;
    }
    play_preroll_done = 0;
    play_underruns = 0;
    play_src_leftover_n = 0;
    play_is_bt = 0;
}

static int name_looks_bluetooth(const char *name)
{
    char buf[256];
    size_t i;
    if (!name)
        return 0;
    for (i = 0; i < sizeof(buf) - 1 && name[i]; i++)
        buf[i] = (char)tolower((unsigned char)name[i]);
    buf[i] = '\0';
    return strstr(buf, "airpods") || strstr(buf, "bluetooth")
        || strstr(buf, "hands-free") || strstr(buf, "headset");
}

static int try_open_stream(int mic_dev, int hp_dev, double rate, int frames, int duplex)
{
    PaStreamParameters in_p, out_p;
    PaError err;
    const PaDeviceInfo *mi = Pa_GetDeviceInfo(mic_dev);
    const PaDeviceInfo *ho = Pa_GetDeviceInfo(hp_dev);

    if (!mi || !ho)
        return -1;

    memset(&in_p, 0, sizeof(in_p));
    memset(&out_p, 0, sizeof(out_p));

    in_channels = (mi->maxInputChannels >= 2) ? 1 : 1; /* always capture mono path */
    if (mi->maxInputChannels < 1)
        return -1;
    /* Prefer mono in; some BT stacks only expose 1. */
    in_channels = 1;

    out_channels = (ho->maxOutputChannels >= 2) ? 2 : 1;
    if (ho->maxOutputChannels < 1)
        return -1;

    in_p.device = mic_dev;
    in_p.channelCount = in_channels;
    in_p.sampleFormat = paFloat32;
    in_p.suggestedLatency = name_looks_bluetooth(mi->name)
        ? mi->defaultHighInputLatency
        : mi->defaultLowInputLatency;

    out_p.device = hp_dev;
    out_p.channelCount = out_channels;
    out_p.sampleFormat = paFloat32;
    out_p.suggestedLatency = name_looks_bluetooth(ho->name)
        ? ho->defaultHighOutputLatency
        : ho->defaultLowOutputLatency;

    duplex_mode = duplex;
    device_rate = rate;
    device_frames = frames;

    if (duplex)
    {
        err = Pa_IsFormatSupported(&in_p, &out_p, rate);
        if (err != paFormatIsSupported)
            return -1;
        err = Pa_OpenStream(&audio_stream, &in_p, &out_p, rate, frames,
                            paClipOff, pa_audio_cb, NULL);
    }
    else
    {
        /* Half-duplex fallback not used — we open duplex or fail into split below. */
        err = paDeviceUnavailable;
    }
    return (err == paNoError) ? 0 : -1;
}

static int start_audio(void)
{
    int mic_dev = Pa_GetDefaultInputDevice();
    int hp_dev = Pa_GetDefaultOutputDevice();
    const PaDeviceInfo *mi;
    const PaDeviceInfo *ho;
    double rates[6];
    int n_rates = 0;
    int i;
    int err_src;
    int frames;
    int same_dev;
    int bt;
    char msg[192];

    if (mic_choice && mic_choice->value() >= 0 && mic_choice->value() < mic_n)
        mic_dev = mic_devs[mic_choice->value()];
    if (hp_choice && hp_choice->value() >= 0 && hp_choice->value() < hp_n)
        hp_dev = hp_devs[hp_choice->value()];

    if (mic_dev < 0 || hp_dev < 0)
        return -1;

    mi = Pa_GetDeviceInfo(mic_dev);
    ho = Pa_GetDeviceInfo(hp_dev);
    if (!mi || !ho)
        return -1;

    same_dev = (mic_dev == hp_dev);
    bt = name_looks_bluetooth(mi->name) || name_looks_bluetooth(ho->name);
    play_is_bt = bt;

    /* AirPods often show up as two PortAudio indices; prefer one endpoint that
     * can do both capture and playback (required for HFP duplex). */
    if (bt && !same_dev)
    {
        int n = Pa_GetDeviceCount();
        int j;
        for (j = 0; j < n; j++)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(j);
            if (!info || !name_looks_bluetooth(info->name))
                continue;
            if (info->maxInputChannels > 0 && info->maxOutputChannels > 0)
            {
                mic_dev = hp_dev = j;
                mi = ho = info;
                same_dev = 1;
                break;
            }
        }
    }

    /* Prefer device native rate first — AirPods often reject fixed 48k duplex. */
    rates[n_rates++] = mi->defaultSampleRate;
    if (fabs(ho->defaultSampleRate - mi->defaultSampleRate) > 0.5)
        rates[n_rates++] = ho->defaultSampleRate;
    rates[n_rates++] = 44100.0;
    rates[n_rates++] = 48000.0;
    rates[n_rates++] = 24000.0;
    rates[n_rates++] = 16000.0;

    if (rb_init(&cap_rb, TTNS_REMOTE_FRAME_SAMPLES * sizeof(float) * 80) != 0)
        return -1;
    cap_rb_ok = 1;
    if (rb_init(&play_rb, TTNS_REMOTE_FRAME_SAMPLES * sizeof(float) * 200) != 0)
    {
        stop_audio();
        return -1;
    }
    play_rb_ok = 1;
    play_preroll_done = 0;
    play_underruns = 0;
    play_src_leftover_n = 0;

    /* Bluetooth wants larger callbacks; wired prefers 512 for lower latency. */
    frames = bt ? 1024 : 512;
    audio_stream = NULL;

    /* Always try duplex on same device (required for AirPods). */
    for (i = 0; i < n_rates && !audio_stream; i++)
    {
        int use_duplex = same_dev || bt;
        int alt_frames;
        if (!use_duplex)
            break;
        if (try_open_stream(mic_dev, hp_dev, rates[i], frames, 1) == 0)
            break;
        alt_frames = (frames == 1024) ? 512 : 1024;
        if (try_open_stream(mic_dev, hp_dev, rates[i], alt_frames, 1) == 0)
            break;
        audio_stream = NULL;
    }

    /* Separate input/output streams when devices differ and are not BT. */
    if (!audio_stream && !same_dev && !bt)
    {
        PaStreamParameters in_p, out_p;
        PaError err;
        memset(&in_p, 0, sizeof(in_p));
        memset(&out_p, 0, sizeof(out_p));
        in_channels = 1;
        out_channels = (ho->maxOutputChannels >= 2) ? 2 : 1;
        in_p.device = mic_dev;
        in_p.channelCount = 1;
        in_p.sampleFormat = paFloat32;
        in_p.suggestedLatency = mi->defaultLowInputLatency;
        out_p.device = hp_dev;
        out_p.channelCount = out_channels;
        out_p.sampleFormat = paFloat32;
        out_p.suggestedLatency = ho->defaultLowOutputLatency;
        duplex_mode = 1; /* still one stream with both ends */
        for (i = 0; i < n_rates; i++)
        {
            device_rate = rates[i];
            device_frames = 512;
            err = Pa_OpenStream(&audio_stream, &in_p, &out_p, rates[i], 512,
                                paClipOff, pa_audio_cb, NULL);
            if (err == paNoError)
                break;
            audio_stream = NULL;
        }
    }

    if (!audio_stream)
    {
        stop_audio();
        snprintf(msg, sizeof(msg),
                 "Could not open mic/phones (PortAudio).\n"
                 "AirPods need the same Mic+Phones device.\n"
                 "Try MacBook mic/speakers, or reconnect AirPods.");
        fl_alert("%s", msg);
        return -1;
    }

    cap_src_ratio = (float)(TTNS_REMOTE_SAMPLERATE / device_rate);
    play_src_ratio = (float)(device_rate / TTNS_REMOTE_SAMPLERATE);
    if (fabs(cap_src_ratio - 1.0f) > 0.001f)
    {
        cap_src = src_new(SRC_SINC_FASTEST, 1, &err_src);
        if (!cap_src)
        {
            stop_audio();
            return -1;
        }
    }
    if (fabs(play_src_ratio - 1.0f) > 0.001f)
    {
        play_src = src_new(SRC_SINC_FASTEST, 1, &err_src);
        if (!play_src)
        {
            stop_audio();
            return -1;
        }
    }

    if (Pa_StartStream(audio_stream) != paNoError)
    {
        stop_audio();
        fl_alert("Audio stream start failed.");
        return -1;
    }

    snprintf(msg, sizeof(msg), "Audio %s @ %.0f Hz",
             same_dev || bt ? "duplex" : "split", device_rate);
    set_status(msg);
    return 0;
}

static void disconnect_now(void)
{
    remote_prefs_capture_ui();
    remote_prefs_save(&g_prefs);

    running = 0;
    connected = 0;
    if (via_wan)
        ttns_wan_client_leave();
    via_wan = 0;
    if (sock_fd >= 0)
    {
        ttns_rnet_send_packet(sock_fd, TTNS_PKT_BYE, (uint8_t)my_slot, 0, NULL, 0);
        ttns_rnet_close(&sock_fd);
    }
    stop_audio();
    if (enc)
    {
        opus_encoder_destroy(enc);
        enc = NULL;
    }
    if (dec)
    {
        opus_decoder_destroy(dec);
        dec = NULL;
    }
    my_slot = -1;
    peer_host[0] = '\0';
    set_status("Disconnected");
    update_connection_ui();
}

static void do_connect(void)
{
    const char *typed;
    char norm_code[12];
    char host[64];
    int port = 0;
    unsigned char hello[64];
    int hellolen;
    ttns_remote_hdr_t hdr;
    unsigned char payload[32];
    int n;
    int err;

    if (connected)
        return;

    my_slot = -1;
    via_wan = 0;
    sock_fd = -1;

    typed = room_input ? room_input->value() : NULL;
    ttns_rnet_normalize_room(norm_code, sizeof(norm_code), typed);
    if (norm_code[0] == '\0' || strlen(norm_code) < 4)
    {
        fl_alert("Enter the room code shown on TTNS Deck (Accept must be on).");
        return;
    }

    if (link_btn)
        link_btn->deactivate();
    set_status("Searching LAN for Deck…");
    Fl::check();

    via_wan = 0;
    sock_fd = -1;
    if (ttns_rnet_discover_room(norm_code, host, sizeof(host), &port, 2500) == 0)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %s:%d — connecting…", host, port);
        set_status(msg);
        Fl::check();

        sock_fd = ttns_rnet_connect(host, port, 3000);
        if (sock_fd < 0)
        {
            set_status("LAN TCP failed — trying internet…");
            Fl::check();
        }
        else
        {
            hellolen = snprintf((char *)hello, sizeof(hello), "%s", norm_code) + 1;
            hellolen += snprintf((char *)hello + hellolen, sizeof(hello) - (size_t)hellolen, "%s",
                                 "Remote") + 1;
            if (ttns_rnet_send_packet(sock_fd, TTNS_PKT_HELLO, 0, 0, hello, (uint16_t)hellolen) != 0
                || (n = ttns_rnet_recv_packet(sock_fd, &hdr, payload, sizeof(payload), 3000)) < 1
                || hdr.type != TTNS_PKT_HELLO_ACK)
            {
                ttns_rnet_close(&sock_fd);
                set_status("LAN handshake failed — trying internet…");
                Fl::check();
            }
            else
            {
                my_slot = payload[0];
                snprintf(peer_host, sizeof(peer_host), "%s", host);
            }
        }
    }

    if (my_slot < 0 || sock_fd < 0)
    {
        set_status("Connecting via internet (WRX)…");
        Fl::check();
        if (!ttns_wan_available()
            || ttns_wan_client_join(norm_code, "Remote", &my_slot) != 0)
        {
            set_status("Connect failed");
            update_connection_ui();
            fl_alert("Could not reach a Deck for room %s.\n\n"
                     "LAN: same Wi‑Fi, Accept on, matching code.\n"
                     "Internet: Deck Accept on, WRX signal up "
                     "(wrx.liveencode.com/ttns).\n%s",
                     norm_code, ttns_wan_last_error());
            return;
        }
        via_wan = 1;
        snprintf(peer_host, sizeof(peer_host), "WAN");
    }

    enc = opus_encoder_create(TTNS_REMOTE_SAMPLERATE, 1, OPUS_APPLICATION_VOIP, &err);
    dec = opus_decoder_create(TTNS_REMOTE_SAMPLERATE, 1, &err);
    if (!enc || !dec)
    {
        disconnect_now();
        fl_alert("Opus init failed");
        return;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(TTNS_REMOTE_OPUS_BR));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    if (start_audio() != 0)
    {
        disconnect_now();
        return;
    }

    connected = 1;
    running = 1;
    tx_seq = 0;
    remote_prefs_save_room(norm_code);
    pthread_create(&rx_thread, NULL, client_rx, NULL);
    pthread_detach(rx_thread);
    pthread_create(&tx_thread, NULL, client_tx, NULL);
    pthread_detach(tx_thread);

    {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "You're live as R%d via %s — mix-minus in phones.",
                 my_slot + 1, via_wan ? "internet" : "LAN");
        set_status(msg);
    }
    update_connection_ui();
    fl_beep();
    fl_message("Connected to the Deck as R%d (%s).\n\n"
               "Mix-minus: you hear music, carts, the Deck mic,\n"
               "and other remotes — but not your own voice.\n\n"
               "Press Disconnect when finished.",
               my_slot + 1, via_wan ? "internet" : "LAN");
}

static void link_toggle_cb(Fl_Widget *, void *)
{
    if (connected)
        disconnect_now();
    else
        do_connect();
}

static void level_cb(Fl_Widget *w, void *which)
{
    float v = (float)((Fl_Slider *)w)->value();
    float g;

    /* Bottom of slider (-24 dB) is hard mute, not −24 dB attenuate. */
    if (v <= -24.0f + 0.001f)
        g = 0.0f;
    else
        g = powf(10.0f, v / 20.0f);

    if ((int)(intptr_t)which == 0)
        mic_gain = g;
    else
        hp_gain = g;
    remote_prefs_capture_ui();
    remote_prefs_save(&g_prefs);
}

static void device_choice_cb(Fl_Widget *w, void *data)
{
    (void)w;
    (void)data;
    remote_prefs_capture_ui();
    remote_prefs_save(&g_prefs);
}

static void fill_devices(void)
{
    int i;
    int n = Pa_GetDeviceCount();
    int def_in = Pa_GetDefaultInputDevice();
    int def_out = Pa_GetDefaultOutputDevice();
    int mic_sel = 0;
    int hp_sel = 0;
    int mic_best = -1;
    int hp_best = -1;
    int mic_score = 0;
    int hp_score = 0;

    mic_n = 0;
    hp_n = 0;
    mic_choice->clear();
    hp_choice->clear();
    for (i = 0; i < n; i++)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        int score;
        if (!info)
            continue;
        if (info->maxInputChannels > 0 && mic_n < 128)
        {
            if (i == def_in)
                mic_sel = mic_n;
            score = name_score_match(g_prefs.mic_name, info->name);
            if (score > mic_score)
            {
                mic_score = score;
                mic_best = mic_n;
            }
            mic_devs[mic_n++] = i;
            mic_choice->add(info->name);
        }
        if (info->maxOutputChannels > 0 && hp_n < 128)
        {
            if (i == def_out)
                hp_sel = hp_n;
            score = name_score_match(g_prefs.hp_name, info->name);
            if (score > hp_score)
            {
                hp_score = score;
                hp_best = hp_n;
            }
            hp_devs[hp_n++] = i;
            hp_choice->add(info->name);
        }
    }
    if (mic_best >= 0)
        mic_sel = mic_best;
    if (hp_best >= 0)
        hp_sel = hp_best;
    if (mic_n > 0)
        mic_choice->value(mic_sel);
    if (hp_n > 0)
        hp_choice->value(hp_sel);
}

static Fl_RGB_Image *load_logo_scaled(int max_h)
{
    char path[PATH_MAX];
    Fl_PNG_Image *src;
    Fl_RGB_Image *scaled;
    int nw;
    int nh;

    if (ttns_path_asset_file("ttns-logo.png", path, sizeof(path)) != 0)
        return NULL;
    src = new Fl_PNG_Image(path);
    if (!src || src->w() <= 0 || src->h() <= 0)
    {
        delete src;
        return NULL;
    }
    nh = max_h;
    nw = (int)((float)src->w() * (float)max_h / (float)src->h());
    if (nw < 1)
        nw = 1;
    scaled = (Fl_RGB_Image *)src->copy(nw, nh);
    delete src;
    return scaled;
}

int main(int argc, char **argv)
{
    Fl_Window *win;
    Fl_Box *title;
    Fl_Box *blurb;
    Fl_Box *lbl;
    Fl_RGB_Image *logo_img;
    int content_x = 12;

    Fl::scheme("none");
    Fl::background(0, 0, 0);
    Fl::background2(0, 0, 0);
    Fl::foreground(51, 255, 51);
    remote_prefs_load(&g_prefs);

    if (Pa_Initialize() != paNoError)
    {
        fl_alert("PortAudio init failed");
        return 1;
    }

    win = new Fl_Window(520, 460, "TTNS Remote");
    win->color(col_bg());

    logo_img = load_logo_scaled(54);
    if (logo_img)
    {
        Fl_Box *logo = new Fl_Box(6, 6, logo_img->w(), logo_img->h());
        logo->image(logo_img);
        logo->box(FL_NO_BOX);
        content_x = 6 + logo_img->w() + 10;
    }

    title = new Fl_Box(content_x, 10, 400 - content_x, 28, "REMOTE");
    title->labelfont(FL_BOLD);
    title->labelsize(18);
    title->labelcolor(col_green());
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    slot_box = new Fl_Box(420, 10, 88, 28, "R—");
    slot_box->box(FL_BORDER_BOX);
    slot_box->color(col_bg());
    slot_box->labelcolor(col_green());
    slot_box->labelfont(FL_BOLD);
    slot_box->labelsize(16);

    core_phone = new Fl_Box(392, 10, 24, 28, "\xE2\x98\x8E"); /* ☎ */
    core_phone->box(FL_NO_BOX);
    core_phone->labelsize(18);
    core_phone->labelcolor(fl_color_average(col_green(), col_bg(), 0.35f));
    core_phone->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    core_phone->tooltip("core.liveencode.com — grey offline, yellow reachable, green connected");

    conn_banner = new Fl_Box(12, 66, 496, 32, "");
    conn_banner->box(FL_BORDER_BOX);
    conn_banner->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    conn_banner->labelsize(13);

    blurb = new Fl_Box(12, 104, 496, 32,
           "Same LAN as the Deck. Accept on Deck → enter room code → Connect. "
           "AirPods: same device for Mic and Phones.");
    blurb->align(FL_ALIGN_WRAP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    blurb->labelcolor(col_green());
    blurb->labelsize(11);

    lbl = new Fl_Box(12, 142, 80, 26, "Mic");
    lbl->labelcolor(col_green());
    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    mic_choice = new Fl_Choice(90, 142, 418, 26, NULL);
    style_choice(mic_choice);
    mic_choice->callback(device_choice_cb);

    lbl = new Fl_Box(12, 174, 80, 26, "Phones");
    lbl->labelcolor(col_green());
    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    hp_choice = new Fl_Choice(90, 174, 418, 26, NULL);
    style_choice(hp_choice);
    hp_choice->callback(device_choice_cb);
    fill_devices();

    lbl = new Fl_Box(12, 212, 80, 28, "Room");
    lbl->labelcolor(col_green());
    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    room_input = new Fl_Input(90, 212, 200, 28, NULL);
    room_input->textsize(16);
    room_input->textfont(FL_BOLD);
    room_input->textcolor(col_red());
    room_input->color(col_dark());
    room_input->box(FL_BORDER_BOX);
    room_input->labelcolor(col_green());
    if (g_prefs.room_code[0])
        room_input->value(g_prefs.room_code);

    link_btn = new Fl_Button(310, 212, 198, 28, "Connect");
    style_btn(link_btn);
    link_btn->labelfont(FL_BOLD);
    link_btn->labelsize(14);
    link_btn->callback(link_toggle_cb);

    lbl = new Fl_Box(12, 256, 80, 22, "Mic level");
    lbl->labelcolor(col_green());
    lbl->labelsize(11);
    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    mic_level = new Fl_Slider(90, 256, 300, 22, NULL);
    style_slider(mic_level);
    mic_level->minimum(-24);
    mic_level->maximum(12);
    mic_level->value(g_prefs.mic_db);
    mic_gain = (g_prefs.mic_db <= -24.0f + 0.001f)
                   ? 0.0f
                   : powf(10.0f, g_prefs.mic_db / 20.0f);
    mic_level->callback(level_cb, (void *)0);
    mic_meter = new TtnsMeter(400, 256, 108, 22);

    lbl = new Fl_Box(12, 286, 80, 22, "Phones");
    lbl->labelcolor(col_green());
    lbl->labelsize(11);
    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    hp_level = new Fl_Slider(90, 286, 300, 22, NULL);
    style_slider(hp_level);
    hp_level->minimum(-24);
    hp_level->maximum(12);
    hp_level->value(g_prefs.hp_db);
    hp_gain = (g_prefs.hp_db <= -24.0f + 0.001f)
                  ? 0.0f
                  : powf(10.0f, g_prefs.hp_db / 20.0f);
    hp_level->callback(level_cb, (void *)1);
    hp_meter = new TtnsMeter(400, 286, 108, 22);

    mic_mute = new Fl_Ttns_Check_Button(90, 324, 140, 24, "Mute mic");
    style_check(mic_mute);
    mic_mute->indicator(TTNS_CHECK_NEGATE);
    mic_mute->callback(mute_opts_cb);
    ptt_mode = new Fl_Ttns_Check_Button(240, 324, 260, 24, "Push-to-talk (hold Ctrl)");
    style_check(ptt_mode);
    ptt_mode->indicator(TTNS_CHECK_AFFIRM);
    ptt_mode->value(0);
    ptt_mode->callback(mute_opts_cb);
    Fl::add_timeout(0.05, ptt_tick);
    Fl::add_timeout(0.05, meter_tick);
    ttns_core_reach_start();
    update_connection_ui();

    status_box = new Fl_Box(12, 360, 496, 84, "");
    status_box->box(FL_BORDER_BOX);
    status_box->color(col_dark());
    status_box->labelcolor(col_green());
    status_box->align(FL_ALIGN_WRAP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_TOP);
    status_box->labelsize(12);
    set_status(status_text);
    update_connection_ui();

    win->end();
    win->show(argc, argv);
    int rc = Fl::run();
    disconnect_now();
    Pa_Terminate();
    return rc;
}
