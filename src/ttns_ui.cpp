#include "ttns_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include "FL/Fl_My_Double_Window.H"
#include "FL/Fl_My_Native_File_Chooser.H"
#include "FL/Fl_My_Value_Slider.H"
#include "FL/Fl_Ttns_Mic_Button.H"
#include "FL/Fl_Ttns_Check_Button.H"
#include "FL/Fl_Ttns_Border_Button.H"
#include "FL/Fl_Ttns_Cart_Button.H"
#include "FL/Fl_Ttns_Fader.H"
#include "cart_player.h"
#include "cfg.h"
#include "fl_callbacks.h"
#include "fl_funcs.h"
#include "flgui.h"
#include "butt.h"
#include "port_audio.h"
#include "ttns_audio.h"
#include "ttns_remote.h"
#include "ttns_remote_session.h"
#include "ttns_remote_wan.h"
#include "ttns_zones.h"
#include "util.h"
#include "ttns_about.h"
#include "ttns_paths.h"
#include "ttns_theme.h"

/* Scaled mixer panel — absolute window coordinates (FLTK convention). */
static const int TTNS_WIN_W = 520;
static const int TTNS_LOGO = 54;
static const int TTNS_LBL_X = 72;
static const int TTNS_VAL_X = 118;
static const int TTNS_ZONE_LIST_W = 180;
static const int TTNS_FADER_H = 22;
static const int TTNS_CART_Y = 168;
static const int TTNS_CART_H = 38;
/* Mixer + carts only — Remotes live BELOW the LCD (More-panel style). */
static const int TTNS_EXTRA_H = TTNS_CART_Y + TTNS_CART_H;
static const int TTNS_REMOTE_HDR_H = 26;
static const int TTNS_REMOTE_ROW_H = 24;
static const int TTNS_REMOTE_ROWS_H =
    TTNS_REMOTE_SLOTS * TTNS_REMOTE_ROW_H + 4;

static const int TTNS_MIC_BTN_X = 6;
static const int TTNS_MIC_BTN_Y = 84;
static const int TTNS_MIC_BTN_W = 58;
static const int TTNS_MIC_BTN_H = 40; /* leave room for Mic mon + Mon mute above carts */

static Fl_Choice *ttns_choice_mount = NULL;
static Fl_Ttns_Check_Button *ttns_chk_mount_confirm = NULL;
static Fl_Box *ttns_duck_lbl = NULL;
static Fl_Ttns_Fader *ttns_slider_mic = NULL;
static Fl_Ttns_Fader *ttns_slider_line = NULL;
static Fl_Ttns_Fader *ttns_slider_cart = NULL;
static Fl_Ttns_Fader *ttns_slider_duck_gate = NULL;
static Fl_Ttns_Fader *ttns_slider_duck_depth = NULL;
static Fl_Box *ttns_duck_led = NULL;
static Fl_Ttns_Check_Button *ttns_chk_monitor_mute = NULL;
static Fl_Ttns_Check_Button *ttns_chk_monitor_master = NULL;
static Fl_Ttns_Mic_Button *ttns_btn_mic = NULL;
static Fl_Ttns_Cart_Button *ttns_cart_btn[TTNS_CART_SLOTS];
static Fl_Ttns_Fader *ttns_slider_remote[TTNS_REMOTE_SLOTS];
static Fl_Ttns_Check_Button *ttns_chk_remote_mute[TTNS_REMOTE_SLOTS];
static Fl_Box *ttns_remote_status[TTNS_REMOTE_SLOTS];
static Fl_Ttns_Check_Button *ttns_chk_remote_accept = NULL;
static Fl_Box *ttns_remote_room_lbl = NULL;
static Fl_Ttns_Border_Button *ttns_btn_remote_newcode = NULL;
static Fl_Button *ttns_btn_remote_test[TTNS_REMOTE_SLOTS];
static Fl_Ttns_Border_Button *ttns_btn_remote_toggle = NULL;
static Fl_Ttns_Check_Button *ttns_chk_ptt_remotes = NULL;
static Fl_Button *ttns_btn_refresh_dev = NULL;
static int ttns_remote_expanded = 0;
static int ttns_seen_dev_epoch = 0;

static Fl_My_Double_Window *ttns_cart_setup_win = NULL;
static Fl_Input *ttns_cart_setup_path = NULL;
static Fl_Round_Button *ttns_cart_setup_oneshot = NULL;
static Fl_Round_Button *ttns_cart_setup_loop = NULL;
static Fl_Ttns_Fader *ttns_cart_setup_gain = NULL;
static int ttns_cart_setup_slot = 0;
static float ttns_cart_setup_gain_undo = 1.0f;

static float ttns_thr_lin_to_db(float lin)
{
    if (lin <= 0.0001f)
        return -50.0f;
    return 20.0f * log10f(lin);
}

static float ttns_thr_db_to_lin(float db)
{
    return powf(10.0f, db / 20.0f);
}

/* Fader bottom (-24 dB) is hard mute; 0 dB stays exact unity. */
static float ttns_slider_db_to_gain(float db)
{
    if (db <= -24.0f + 0.001f)
        return 0.0f;
    if ((int)db == 0)
        return 1.0f;
    return util_db_to_factor(db);
}

static float ttns_gain_to_slider_db(float gain)
{
    float db;

    if (gain <= 0.0f)
        return -24.0f;
    db = util_factor_to_db(gain);
    if (db < -24.0f)
        return -24.0f;
    if (db > 24.0f)
        return 24.0f;
    return db;
}

static void ttns_style_duck_slider(Fl_My_Value_Slider *s, double min_db, double max_db)
{
    ttns_theme_style_slider(s);
    s->labeltype(FL_NO_LABEL);
    s->minimum(min_db);
    s->maximum(max_db);
    s->step(0.5);
    s->when(FL_WHEN_CHANGED);
}

static void ttns_style_slider(Fl_My_Value_Slider *s)
{
    ttns_theme_style_slider(s);
    s->labeltype(FL_NO_LABEL);
    s->minimum(-24);
    s->maximum(24);
    s->step(0.1);
    s->when(FL_WHEN_CHANGED);
    s->labelsize(12);
}

static Fl_Box *ttns_lbl(int x, int y, int w, const char *text)
{
    Fl_Box *b = new Fl_Box(x, y, w, 18, text);
    b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    b->labelfont(1);
    b->labelsize(12);
    b->box(FL_NO_BOX);
    ttns_theme_style_label_box(b);
    return b;
}

static void ttns_style_choice(Fl_Choice *c)
{
    ttns_theme_style_choice(c);
    c->labeltype(FL_NO_LABEL);
    c->textsize(11);
}

static void ttns_style_check(Fl_Ttns_Check_Button *c)
{
    ttns_theme_style_check(c);
}

static void ttns_fill_cfg_audio_devices(void)
{
    int i;

    if (!fl_g || !fl_g->choice_cfg_dev || !fl_g->choice_cfg_ttns_mic)
        return;

    fl_g->choice_cfg_dev->clear();
    fl_g->choice_cfg_ttns_mic->clear();
    for (i = 0; i < cfg.audio.dev_count; i++)
    {
        fl_g->choice_cfg_dev->add(cfg.audio.pcm_list[i]->name);
        fl_g->choice_cfg_ttns_mic->add(cfg.audio.pcm_list[i]->name);
    }
    if (cfg.audio.dev_count > 0)
    {
        fl_g->choice_cfg_dev->value(cfg.ttns.line_dev_num);
        fl_g->choice_cfg_ttns_mic->value(cfg.ttns.mic_dev_num);
    }

    if (fl_g->choice_cfg_ttns_monitor_out)
    {
        fl_g->choice_cfg_ttns_monitor_out->clear();
        for (i = 0; i < cfg.audio.out_dev_count; i++)
            fl_g->choice_cfg_ttns_monitor_out->add(cfg.audio.out_pcm_list[i]->name);
        if (cfg.audio.out_dev_count > 0)
            fl_g->choice_cfg_ttns_monitor_out->value(cfg.ttns.monitor_out_dev_num);
    }
}

static void ttns_raise_duck_led(flgui *g)
{
    if (!g || !g->window_main || !ttns_duck_led)
        return;

    g->window_main->remove(ttns_duck_led);
    g->window_main->add(ttns_duck_led);
}

static void ttns_update_cart_labels(void)
{
    int i;
    char lbl[16];
    const char *name;

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        if (!ttns_cart_btn[i])
            continue;
        name = ttns_cart_get_label(i);
        if (!name && cfg.ttns.cart_label[i] && cfg.ttns.cart_label[i][0])
            name = cfg.ttns.cart_label[i];
        if (name && name[0])
            snprintf(lbl, sizeof(lbl), "%.4s", name);
        else
            snprintf(lbl, sizeof(lbl), "%d", i + 1);
        ttns_cart_btn[i]->copy_label(lbl);
        ttns_cart_btn[i]->redraw();
    }
}

static Fl_RGB_Image *ttns_load_logo_scaled(int max_h)
{
    char path[PATH_MAX];
    Fl_PNG_Image *src = NULL;
    Fl_RGB_Image *scaled = NULL;
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

    scaled = (Fl_RGB_Image*)src->copy(nw, nh);
    delete src;
    return scaled;
}

static void ttns_about_cb(Fl_Widget *, void *)
{
    ttns_show_about();
}

static void ttns_reopen_audio_deferred(void);
static void ttns_reopen_mic_deferred(void);
static void ttns_reopen_monitor_deferred(void);
static void ttns_full_reopen_timeout(void *);
static void ttns_mic_reopen_timeout(void *);
static void ttns_monitor_reopen_timeout(void *);
static void ttns_reopen_debounce_timeout(void *);
static void ttns_sync_audio_devices_from_ui(void);
static int ttns_applied_line_dev = -1;
static int ttns_applied_mic_dev = -1;
static int ttns_applied_monitor_out = -1;
static int ttns_applied_mic_monitor = -1;
static char ttns_audio_reopen_pending = 0;
static char ttns_audio_reopen_queued = 0;
static char ttns_audio_reopen_busy = 0;

void ttns_audio_mark_applied(void)
{
    ttns_applied_line_dev = cfg.ttns.line_dev_num;
    ttns_applied_mic_dev = cfg.ttns.mic_dev_num;
    ttns_applied_monitor_out = cfg.ttns.monitor_out_dev_num;
    ttns_applied_mic_monitor = cfg.ttns.mic_monitor;
}

static int ttns_applied_dual_mic(void)
{
    return (ttns_applied_line_dev >= 0 && ttns_applied_mic_dev >= 0
            && ttns_applied_line_dev != ttns_applied_mic_dev);
}

static int ttns_wants_dual_mic(void)
{
    return cfg.ttns.line_dev_num != cfg.ttns.mic_dev_num;
}

static int ttns_input_unchanged(void)
{
    if (ttns_applied_line_dev < 0)
        return 0;

    return (cfg.ttns.line_dev_num == ttns_applied_line_dev
            && cfg.ttns.mic_dev_num == ttns_applied_mic_dev
            && ttns_wants_dual_mic() == ttns_applied_dual_mic());
}

static int ttns_monitor_unchanged(void)
{
    return (cfg.ttns.monitor_out_dev_num == ttns_applied_monitor_out
            && cfg.ttns.mic_monitor == ttns_applied_mic_monitor);
}

static int ttns_can_mic_only_reopen(void)
{
    if (ttns_applied_line_dev < 0)
        return 0;
    if (cfg.ttns.line_dev_num != ttns_applied_line_dev)
        return 0;
    if (ttns_wants_dual_mic() != ttns_applied_dual_mic())
        return 0;
    if (cfg.ttns.mic_dev_num == ttns_applied_mic_dev)
        return 0;
    return ttns_wants_dual_mic();
}

void ttns_audio_settings_changed(void)
{
    unsaved_changes = 1;
    while (Fl::has_timeout(ttns_reopen_debounce_timeout))
        Fl::remove_timeout(ttns_reopen_debounce_timeout);
    Fl::add_timeout(0.45, ttns_reopen_debounce_timeout);
}

static void ttns_full_reopen_timeout(void *)
{
    snd_reinit();
    ttns_audio_reopen_busy = 0;
    ttns_audio_reopen_pending = 0;

    if (snd_audio_is_active())
    {
        ttns_audio_mark_applied();
        ttns_mixer_reset();
        print_info("Audio devices ready", 0);
    }
    else
        print_info("Audio reopen failed — try another device or samplerate in Settings", 1);

    if (ttns_audio_reopen_queued)
    {
        ttns_audio_reopen_queued = 0;
        ttns_apply_audio_settings();
    }
}

static void ttns_mic_reopen_timeout(void *)
{
    int r;

    if (!snd_audio_is_active())
    {
        ttns_audio_reopen_busy = 0;
        ttns_audio_reopen_pending = 0;
        ttns_reopen_audio_deferred();
        return;
    }

    r = snd_reopen_mic_only();
    if (r != 0)
    {
        /* Mic-only reopen leaves capture stopped on failure — recover with full reopen. */
        print_info("Mic reopen failed — restarting all audio…", 1);
        snd_reinit();
        r = snd_audio_is_active() ? 0 : 1;
    }

    ttns_audio_reopen_busy = 0;
    ttns_audio_reopen_pending = 0;

    if (r == 0)
    {
        ttns_audio_mark_applied();
        ttns_mixer_reset();
        print_info("Mic device ready", 0);
    }
    else
        print_info("Mic reopen failed — try another device in Settings", 1);

    if (ttns_audio_reopen_queued)
    {
        ttns_audio_reopen_queued = 0;
        ttns_apply_audio_settings();
    }
}

static void ttns_monitor_reopen_timeout(void *)
{
    if (!snd_audio_is_active())
    {
        ttns_audio_reopen_busy = 0;
        ttns_audio_reopen_pending = 0;
        ttns_reopen_audio_deferred();
        return;
    }

    snd_reopen_monitor();
    ttns_audio_reopen_busy = 0;
    ttns_audio_reopen_pending = 0;

    if (snd_audio_is_active() && (!cfg.ttns.mic_monitor || snd_monitor_is_open()))
    {
        ttns_audio_mark_applied();
        ttns_mixer_reset();
        print_info(cfg.ttns.mic_monitor ? "Monitor output ready" : "Monitor output off", 0);
    }
    else
        print_info("Monitor reopen failed — try another device in Settings", 1);

    if (ttns_audio_reopen_queued)
    {
        ttns_audio_reopen_queued = 0;
        ttns_apply_audio_settings();
    }
}

static void ttns_reopen_audio_deferred(void)
{
    if (ttns_audio_reopen_pending || ttns_audio_reopen_busy)
    {
        ttns_audio_reopen_queued = 1;
        return;
    }
    ttns_audio_reopen_pending = 1;
    ttns_audio_reopen_busy = 1;
    print_info("Applying audio devices…", 0);
    Fl::add_timeout(0.0, ttns_full_reopen_timeout);
}

static void ttns_reopen_debounce_timeout(void *)
{
    ttns_apply_audio_settings();
}

void ttns_schedule_audio_reopen(void)
{
    ttns_audio_settings_changed();
}

void ttns_apply_audio_settings(void)
{
    int input_changed;
    int monitor_changed;

    ttns_sync_audio_devices_from_ui();
    while (Fl::has_timeout(ttns_reopen_debounce_timeout))
        Fl::remove_timeout(ttns_reopen_debounce_timeout);

    input_changed = !ttns_input_unchanged();
    monitor_changed = !ttns_monitor_unchanged();

    if (!input_changed && !monitor_changed)
        return;

    if (input_changed)
    {
        if (ttns_can_mic_only_reopen())
            ttns_reopen_mic_deferred();
        else
            ttns_reopen_audio_deferred();
        return;
    }

    ttns_reopen_monitor_deferred();
}

static void ttns_sync_audio_devices_from_ui(void)
{
    if (fl_g && fl_g->choice_cfg_dev)
    {
        cfg.ttns.line_dev_num = fl_g->choice_cfg_dev->value();
        cfg.audio.dev_num = cfg.ttns.line_dev_num;
    }
    if (fl_g && fl_g->choice_cfg_ttns_mic)
        cfg.ttns.mic_dev_num = fl_g->choice_cfg_ttns_mic->value();
    if (fl_g && fl_g->choice_cfg_ttns_monitor_out)
        cfg.ttns.monitor_out_dev_num = fl_g->choice_cfg_ttns_monitor_out->value();
}

static void ttns_reopen_monitor_deferred(void)
{
    if (ttns_audio_reopen_pending || ttns_audio_reopen_busy)
    {
        ttns_audio_reopen_queued = 1;
        return;
    }
    ttns_audio_reopen_pending = 1;
    ttns_audio_reopen_busy = 1;
    print_info("Applying monitor output…", 0);
    Fl::add_timeout(0.0, ttns_monitor_reopen_timeout);
}

static void ttns_reopen_mic_deferred(void)
{
    if (ttns_audio_reopen_pending || ttns_audio_reopen_busy)
    {
        ttns_audio_reopen_queued = 1;
        return;
    }
    ttns_audio_reopen_pending = 1;
    ttns_audio_reopen_busy = 1;
    print_info("Applying mic device…", 0);
    Fl::add_timeout(0.0, ttns_mic_reopen_timeout);
}

static void ttns_dev_cb(Fl_Widget *w, void *which)
{
    (void)which;
    if (!fl_g)
        return;

    if (w == (Fl_Widget*)fl_g->choice_cfg_dev)
    {
        cfg.ttns.line_dev_num = fl_g->choice_cfg_dev->value();
        cfg.audio.dev_num = cfg.ttns.line_dev_num;
        if (cfg.audio.dev_num >= 0 && cfg.audio.dev_num < cfg.audio.dev_count)
            update_samplerates();
    }
    else if (w == (Fl_Widget*)fl_g->choice_cfg_ttns_mic)
        cfg.ttns.mic_dev_num = fl_g->choice_cfg_ttns_mic->value();
    else if (w == (Fl_Widget*)fl_g->choice_cfg_ttns_monitor_out)
    {
        int n = fl_g->choice_cfg_ttns_monitor_out->value();
        cfg.ttns.monitor_out_dev_num = n;
        if (n >= 0 && n < cfg.audio.out_dev_count && cfg.audio.out_pcm_list != NULL
            && cfg.audio.out_pcm_list[n]->dev_id != TTNS_MONITOR_OFF)
        {
            cfg.ttns.mic_monitor = 1;
            free(cfg.ttns.monitor_out_name);
            cfg.ttns.monitor_out_name =
                cfg.audio.out_pcm_list[n]->name
                    ? strdup(cfg.audio.out_pcm_list[n]->name) : NULL;
        }
        else
        {
            cfg.ttns.mic_monitor = 0;
            free(cfg.ttns.monitor_out_name);
            cfg.ttns.monitor_out_name = NULL;
        }
    }

    cfg_capture_audio_device_names();
    ttns_audio_settings_changed();
}

static void ttns_ptt_remotes_cb(Fl_Widget *, void *)
{
    int on = (ttns_chk_ptt_remotes && ttns_chk_ptt_remotes->value()) ? 1 : 0;

    ttns_ptt_remotes_set(on);
    if (on)
        print_info("PTT remotes — host and guests off-air (program not ducked)", 0);
    else
        print_info("PTT remotes off — voices back on-air", 0);
}

static void ttns_refresh_devices_timeout(void *)
{
    int r;

    print_info("Rescanning audio devices…", 0);
    r = snd_refresh_devices();
    ttns_fill_cfg_audio_devices();
    update_samplerates();
    ttns_seen_dev_epoch = snd_device_list_epoch();
    ttns_audio_reopen_busy = 0;
    ttns_audio_reopen_pending = 0;

    if (r == 0 && snd_audio_is_active())
    {
        ttns_audio_mark_applied();
        print_info("Audio devices ready", 0);
    }
    else
        print_info("Audio rescan failed — try another device in Settings", 1);

    if (ttns_audio_reopen_queued)
    {
        ttns_audio_reopen_queued = 0;
        ttns_apply_audio_settings();
    }
}

static void ttns_refresh_dev_cb(Fl_Widget *, void *)
{
    if (ttns_audio_reopen_pending || ttns_audio_reopen_busy)
    {
        ttns_audio_reopen_queued = 1;
        print_info("Device refresh queued…", 0);
        return;
    }
    ttns_audio_reopen_pending = 1;
    ttns_audio_reopen_busy = 1;
    Fl::add_timeout(0.0, ttns_refresh_devices_timeout);
}

static void ttns_gain_cb(Fl_Widget *w, void *which)
{
    float db = (float)((Fl_My_Value_Slider*)w)->value();
    float factor = ttns_slider_db_to_gain(db);
    int bus = (int)(intptr_t)which;

    if (bus == 1)
        cfg.ttns.line_gain = factor;
    else if (bus == 2)
        cfg.ttns.cart_gain = factor;
    else
        cfg.ttns.mic_gain = factor;

    unsaved_changes = 1;
}

static void ttns_duck_gate_cb(Fl_Widget *w, void *)
{
    float db = (float)((Fl_My_Value_Slider*)w)->value();
    cfg.ttns.duck_threshold = ttns_thr_db_to_lin(db);
    unsaved_changes = 1;
}

static void ttns_duck_depth_cb(Fl_Widget *w, void *)
{
    cfg.ttns.duck_depth_db = (float)((Fl_My_Value_Slider*)w)->value();
    unsaved_changes = 1;
}

static void ttns_remote_gain_cb(Fl_Widget *w, void *which)
{
    float db = (float)((Fl_My_Value_Slider*)w)->value();
    float factor = ttns_slider_db_to_gain(db);
    int slot = (int)(intptr_t)which;

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    cfg.ttns.remote_gain[slot] = factor;
    unsaved_changes = 1;
}

static int ttns_remote_any_live(void)
{
    int i;

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (ttns_remote_is_live(i))
            return 1;
    }
    return 0;
}

/*
 * Place Remotes under the LCD deck and size the window like the More panel:
 * hide/show the channel rows, move info_output, resize the window.
 * Never move the deck/LCD group (that caused the mixer overlap).
 */
static const int TTNS_INFO_PANEL_H = 184;

static Fl_Box *ttns_core_phone = NULL;

static void ttns_remote_place_header(int y, int win_w)
{
    const int newcode_w = 78;
    const int newcode_x = win_w - 8 - newcode_w;
    const int code_gap = 2;
    const int phone_w = 22;
    const int ptt_w = 50;
    int code_w = 96;
    int code_x;
    const char *code_txt;
    int accept_x = 100;
    int ptt_x;

    if (ttns_btn_remote_toggle)
        ttns_btn_remote_toggle->resize(8, y, 88, 24);
    if (ttns_chk_remote_accept)
        ttns_chk_remote_accept->resize(accept_x, y, 72, 24);
    if (ttns_core_phone)
        ttns_core_phone->resize(accept_x + 74, y + 2, phone_w, 20);
    ptt_x = accept_x + 74 + phone_w + 4;
    if (ttns_chk_ptt_remotes)
        ttns_chk_ptt_remotes->resize(ptt_x, y, ptt_w, 24);
    if (ttns_btn_remote_newcode)
        ttns_btn_remote_newcode->resize(newcode_x, y, newcode_w, 24);
    if (ttns_remote_room_lbl)
    {
        code_txt = ttns_remote_room_lbl->label();
        if (code_txt && *code_txt)
        {
            fl_font(ttns_remote_room_lbl->labelfont(), ttns_remote_room_lbl->labelsize());
            code_w = (int)fl_width(code_txt) + 4;
            if (code_w < 72)
                code_w = 72;
        }
        code_x = newcode_x - code_gap - code_w;
        ttns_remote_room_lbl->resize(code_x, y, code_w, 24);
        ttns_remote_room_lbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    }
}

static void ttns_remote_relayout(void)
{
    Fl_Widget *deck;
    Fl_Window *win;
    int win_w;
    int y;
    int body_h;
    int remotes_bottom;
    int h;
    int i;
    int mute_w = 48;
    int test_w = 28;
    int status_w = 36;
    int fad_x;
    int fad_w;
    int row_y;
    int info_h;

    if (!fl_g || !fl_g->window_main || !fl_g->lcd || !ttns_btn_remote_toggle)
        return;

    win = fl_g->window_main;
    deck = fl_g->lcd->parent();
    if (!deck)
        return;

    win_w = win->w();
    info_h = TTNS_INFO_PANEL_H;
    if (fl_g->info_output && fl_g->info_output->h() > 40)
        info_h = fl_g->info_output->h();

    /*
     * Pin deck geometry first (window resize reflows children and would
     * shove Play under the VU). Then park Remotes under the stable deck.
     */
    win->resizable(NULL);
    win->size_range(TTNS_WIN_W, 50, TTNS_WIN_W);
    ttns_layout_feedback_panel();

    y = deck->y() + deck->h() + 6;
    body_h = TTNS_REMOTE_HDR_H;
    if (ttns_remote_expanded)
        body_h += TTNS_REMOTE_ROWS_H;

    ttns_remote_place_header(y, win_w);

    fad_x = 8 + mute_w + 4;
    fad_w = win_w - fad_x - status_w - test_w - 14;

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        row_y = y + TTNS_REMOTE_HDR_H + i * TTNS_REMOTE_ROW_H;

        if (ttns_chk_remote_mute[i])
            ttns_chk_remote_mute[i]->resize(8, row_y, mute_w, 22);
        if (ttns_slider_remote[i])
            ttns_slider_remote[i]->resize(fad_x, row_y, fad_w, TTNS_FADER_H);
        if (ttns_btn_remote_test[i])
            ttns_btn_remote_test[i]->resize(fad_x + fad_w + 2, row_y, test_w, 22);
        if (ttns_remote_status[i])
            ttns_remote_status[i]->resize(fad_x + fad_w + test_w + 4, row_y, status_w, 22);

        if (ttns_remote_expanded)
        {
            if (ttns_chk_remote_mute[i]) ttns_chk_remote_mute[i]->show();
            if (ttns_slider_remote[i]) ttns_slider_remote[i]->show();
            if (ttns_btn_remote_test[i]) ttns_btn_remote_test[i]->show();
            if (ttns_remote_status[i]) ttns_remote_status[i]->show();
        }
        else
        {
            if (ttns_chk_remote_mute[i]) ttns_chk_remote_mute[i]->hide();
            if (ttns_slider_remote[i]) ttns_slider_remote[i]->hide();
            if (ttns_btn_remote_test[i]) ttns_btn_remote_test[i]->hide();
            if (ttns_remote_status[i]) ttns_remote_status[i]->hide();
        }
    }

    remotes_bottom = y + body_h;

    if (fl_g->info_output)
        fl_g->info_output->resize(0, remotes_bottom + 4, win_w, info_h);

    if (fl_g->info_visible && fl_g->info_output && fl_g->info_output->visible())
        h = remotes_bottom + 4 + info_h;
    else
        h = remotes_bottom + 8;

    ttns_set_window_collapsed_height(remotes_bottom + 8);

    win->resize(win->x(), win->y(), TTNS_WIN_W, h);
    /* Re-pin deck once more — the resize above can nudge children. */
    ttns_layout_feedback_panel();
    y = deck->y() + deck->h() + 6;
    ttns_remote_place_header(y, win_w);
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        row_y = y + TTNS_REMOTE_HDR_H + i * TTNS_REMOTE_ROW_H;
        if (ttns_chk_remote_mute[i])
            ttns_chk_remote_mute[i]->resize(8, row_y, mute_w, 22);
        if (ttns_slider_remote[i])
            ttns_slider_remote[i]->resize(fad_x, row_y, fad_w, TTNS_FADER_H);
        if (ttns_btn_remote_test[i])
            ttns_btn_remote_test[i]->resize(fad_x + fad_w + 2, row_y, test_w, 22);
        if (ttns_remote_status[i])
            ttns_remote_status[i]->resize(fad_x + fad_w + test_w + 4, row_y, status_w, 22);
    }
    remotes_bottom = y + body_h;
    if (fl_g->info_output)
        fl_g->info_output->resize(0, remotes_bottom + 4, win_w, info_h);
    ttns_set_window_collapsed_height(remotes_bottom + 8);

    if (fl_g->info_visible && fl_g->info_output && fl_g->info_output->visible())
        h = remotes_bottom + 4 + info_h;
    else
        h = remotes_bottom + 8;
    if (win->h() != h)
        win->resize(win->x(), win->y(), TTNS_WIN_W, h);

    win->init_sizes();
    if (fl_g->info_output)
        win->resizable(fl_g->info_output);
    win->size_range(TTNS_WIN_W, 50, TTNS_WIN_W);
    win->redraw();
}

void ttns_ui_relayout_shell(void)
{
    ttns_remote_relayout();
}

static void ttns_remote_set_expanded(int want)
{
    want = want ? 1 : 0;
    if (want == ttns_remote_expanded && ttns_btn_remote_toggle)
    {
        /* Still relayout in case deck moved (e.g. after theme/layout). */
        ttns_remote_relayout();
        return;
    }
    if (!fl_g || !fl_g->window_main)
        return;

    ttns_remote_expanded = want;

    if (ttns_btn_remote_toggle)
    {
        ttns_btn_remote_toggle->copy_label(want ? "Remotes@2>" : "Remotes@>");
        ttns_btn_remote_toggle->redraw();
    }

    ttns_remote_relayout();
}

static void ttns_remote_toggle_cb(Fl_Widget *, void *)
{
    ttns_remote_set_expanded(!ttns_remote_expanded);
}

static void ttns_remote_sync_expand(void)
{
    /* Show strip while accepting guests or anyone is connected; else tuck away. */
    if (cfg.ttns.remote_accept || ttns_remote_any_live())
        ttns_remote_set_expanded(1);
    else
        ttns_remote_set_expanded(0);
}

static void ttns_remote_mute_cb(Fl_Widget *w, void *which)
{
    int slot = (int)(intptr_t)which;
    Fl_Ttns_Check_Button *b = (Fl_Ttns_Check_Button *)w;

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    cfg.ttns.remote_mute[slot] = b->value() ? 1 : 0;
    unsaved_changes = 1;
}

static void ttns_remote_accept_cb(Fl_Widget *w, void *)
{
    Fl_Ttns_Check_Button *b = (Fl_Ttns_Check_Button *)w;
    cfg.ttns.remote_accept = b->value() ? 1 : 0;
    if (cfg.ttns.remote_accept && cfg.ttns.remote_room[0] == '\0')
    {
        char code[TTNS_REMOTE_ROOM_LEN];
        ttns_remote_generate_room_code(code, sizeof(code));
        ttns_remote_set_room_code(code);
        if (ttns_remote_room_lbl)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "Code %s", ttns_remote_room_code());
            ttns_remote_room_lbl->copy_label(buf);
        }
    }
    if (cfg.ttns.remote_accept)
        ttns_remote_session_host_start();
    else
        ttns_remote_session_host_stop();
    ttns_remote_sync_expand();
    unsaved_changes = 1;
}

static void ttns_remote_newcode_cb(Fl_Widget *, void *)
{
    char code[TTNS_REMOTE_ROOM_LEN];
    ttns_remote_generate_room_code(code, sizeof(code));
    ttns_remote_set_room_code(code);
    if (ttns_remote_room_lbl)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Code %s", ttns_remote_room_code());
        ttns_remote_room_lbl->copy_label(buf);
        ttns_remote_room_lbl->redraw();
    }
    if (ttns_remote_session_host_running())
        ttns_remote_session_host_refresh_discovery();
    unsaved_changes = 1;
}

static void ttns_remote_style_test_button(int slot)
{
    Fl_Button *b;
    int on;

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;
    b = ttns_btn_remote_test[slot];
    if (!b)
        return;

    on = ttns_remote_test_tone(slot);
    if (on)
    {
        b->copy_label("T*");
        b->labelcolor(ttns_col_bg());
        b->color(ttns_col_green());
        b->selection_color(ttns_col_green());
    }
    else
    {
        b->copy_label("T");
        ttns_theme_style_butt_button(b, 0);
    }
    b->redraw();
}

static void ttns_remote_test_cb(Fl_Widget *, void *which)
{
    int slot = (int)(intptr_t)which;
    char msg[192];

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return;

    if (ttns_remote_test_tone(slot))
    {
        ttns_remote_set_test_tone(slot, 0);
        ttns_remote_clear_slot(slot);
        snprintf(msg, sizeof(msg), "Remote R%d test tone OFF", slot + 1);
        print_info(msg, 0);
    }
    else
    {
        if (!snd_audio_is_active() || !ttns_remote_is_inited())
        {
            print_info("Remote test needs audio input open — check Settings → Audio devices", 1);
            return;
        }

        ttns_remote_set_name(slot, "Test tone");
        ttns_remote_set_test_tone(slot, 1);

        if (!cfg.ttns.mic_monitor)
        {
            snprintf(msg, sizeof(msg),
                     "Remote R%d test tone ON (in the mix). To hear it: Settings → "
                     "Monitor Output = your headphones, then Save.",
                     slot + 1);
            print_info(msg, 1);
        }
        else
        {
            snprintf(msg, sizeof(msg),
                     "Remote R%d test tone ON — should be audible on Monitor Output "
                     "(and will duck Line/Cart if Gate allows)",
                     slot + 1);
            print_info(msg, 0);
        }
    }

    ttns_remote_style_test_button(slot);
    ttns_remote_sync_expand();
}

static void ttns_mic_apply_mute(int muted)
{
    cfg.ttns.mic_mute = muted ? 1 : 0;
    if (ttns_btn_mic)
    {
        ttns_btn_mic->value(muted ? 1 : 0);
        ttns_btn_mic->redraw();
    }
    unsaved_changes = 1;
}

static void ttns_mic_toggle(void)
{
    ttns_mic_apply_mute(cfg.ttns.mic_mute ? 0 : 1);
}

static void ttns_mic_mute_cb(Fl_Widget *w, void *)
{
    (void)w;
    ttns_mic_apply_mute(((Fl_Button*)w)->value() ? 1 : 0);
}

static void ttns_monitor_cb(Fl_Widget *w, void *)
{
    (void)w;
    cfg.ttns.mic_monitor_mute = (ttns_chk_monitor_mute && ttns_chk_monitor_mute->value())
        ? 0 : 1;
    unsaved_changes = 1;
}

static void ttns_monitor_master_cb(Fl_Widget *w, void *)
{
    (void)w;
    cfg.ttns.monitor_mute = (ttns_chk_monitor_master && ttns_chk_monitor_master->value())
        ? 1 : 0;
    unsaved_changes = 1;
}

static void ttns_zone_cb(Fl_Widget *, void *)
{
    /* Changing mount clears Confirm — DJ must re-check before going live. */
    ttns_ui_clear_mount_confirm();
    ttns_ui_apply_zone_selection();
    ttns_ui_sync_advanced();
    unsaved_changes = 1;
}

void ttns_ui_clear_mount_confirm(void)
{
    if (ttns_chk_mount_confirm)
    {
        ttns_chk_mount_confirm->value(0);
        ttns_chk_mount_confirm->redraw();
    }
    ttns_ui_update_connect_armed();
}

int ttns_ui_mount_is_confirmed(void)
{
    return (ttns_chk_mount_confirm && ttns_chk_mount_confirm->value()) ? 1 : 0;
}

void ttns_ui_update_connect_armed(void)
{
    Fl_Button *btn;

    if (!fl_g || !fl_g->button_connect)
        return;

    btn = fl_g->button_connect;
    if (connected)
    {
        btn->deactivate();
        btn->tooltip("Already connected — use Stop to disconnect");
    }
    else if (ttns_ui_mount_is_confirmed())
    {
        btn->activate();
        btn->tooltip("Go live (Mount confirmed)");
    }
    else
    {
        btn->deactivate();
        btn->tooltip("Tick Confirm next to Mount before going live");
    }
    btn->redraw();
}

static void ttns_mount_confirm_cb(Fl_Widget *, void *)
{
    ttns_ui_update_connect_armed();
}

void ttns_ui_sync_advanced(void)
{
    if (!fl_g)
        return;

    fill_cfg_widgets();
    choice_cfg_act_srv_cb();
    choice_cfg_act_icy_cb();
}

static void ttns_cart_apply_path(int slot, const char *picked)
{
    const char *base;
    char label[32];

    if (ttns_cart_load(slot, picked) != 0)
    {
        fl_alert("Could not load audio:\n%s\n(Supported: WAV, MP3, M4A, FLAC, OGG)", picked);
        return;
    }

    if (cfg.ttns.cart_path[slot])
        free(cfg.ttns.cart_path[slot]);
    cfg.ttns.cart_path[slot] = strdup(picked);

    base = strrchr(picked, '/');
    if (!base)
        base = strrchr(picked, '\\');
    if (base)
        base++;
    else
        base = picked;

    snprintf(label, sizeof(label), "%s", base);
    if (cfg.ttns.cart_label[slot])
        free(cfg.ttns.cart_label[slot]);
    cfg.ttns.cart_label[slot] = strdup(label);
    ttns_cart_set_label(slot, label);
    ttns_update_cart_labels();
}

#define TTNS_CART_AUDIO_FILTER \
    "Audio Files\t*.{wav,mp3,m4a,flac,ogg}\n" \
    "All Files\t*"

static void ttns_cart_setup_browse_cb(Fl_Widget *, void *)
{
    Fl_My_Native_File_Chooser nfc;
    const char *picked;

    nfc.title("Select cart audio");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    nfc.filter(TTNS_CART_AUDIO_FILTER);
    if (nfc.show() != 0)
        return;

    picked = nfc.filename();
    if (!picked || !picked[0])
        return;

    ttns_cart_setup_path->value(picked);
}

static void ttns_cart_setup_gain_cb(Fl_Widget *w, void *)
{
    int slot = ttns_cart_setup_slot;
    float db = (float)((Fl_Ttns_Fader*)w)->value();
    float factor = ttns_slider_db_to_gain(db);

    cfg.ttns.cart_slot_gain[slot] = factor;
    ttns_cart_set_gain(slot, factor);
    unsaved_changes = 1;

    if (ttns_cart_has_audio(slot) && !ttns_cart_is_playing(slot))
        ttns_cart_trigger(slot);
}

static void ttns_cart_setup_ok_cb(Fl_Widget *, void *)
{
    int slot = ttns_cart_setup_slot;
    const char *path;
    int mode;

    path = ttns_cart_setup_path->value();
    mode = ttns_cart_setup_loop->value() ? TTNS_CART_LOOP : TTNS_CART_ONESHOT;

    cfg.ttns.cart_mode[slot] = mode;
    ttns_cart_set_mode(slot, mode);

    if (ttns_cart_setup_gain)
    {
        float db = (float)ttns_cart_setup_gain->value();
        float factor = ttns_slider_db_to_gain(db);

        cfg.ttns.cart_slot_gain[slot] = factor;
        ttns_cart_set_gain(slot, factor);
    }

    if (path && path[0])
        ttns_cart_apply_path(slot, path);
    else
    {
        ttns_cart_clear(slot);
        if (cfg.ttns.cart_path[slot])
        {
            free(cfg.ttns.cart_path[slot]);
            cfg.ttns.cart_path[slot] = strdup("");
        }
        if (cfg.ttns.cart_label[slot])
        {
            free(cfg.ttns.cart_label[slot]);
            cfg.ttns.cart_label[slot] = strdup("");
        }
        ttns_cart_set_label(slot, NULL);
        ttns_update_cart_labels();
    }

    unsaved_changes = 1;
    ttns_cart_setup_win->hide();
}

static void ttns_cart_setup_cancel_cb(Fl_Widget *, void *)
{
    int slot = ttns_cart_setup_slot;

    cfg.ttns.cart_slot_gain[slot] = ttns_cart_setup_gain_undo;
    ttns_cart_set_gain(slot, ttns_cart_setup_gain_undo);
    ttns_cart_setup_win->hide();
}

static void ttns_cart_setup_show(int slot)
{
    char title[32];

    if (!ttns_cart_setup_win)
    {
        ttns_cart_setup_win = new Fl_My_Double_Window(440, 220, "Cart setup");
        ttns_cart_setup_win->set_modal();
        ttns_theme_style_window(ttns_cart_setup_win);

        ttns_lbl(12, 12, 80, "Audio file");
        ttns_cart_setup_path = new Fl_Input(12, 28, 320, 22);
        ttns_cart_setup_path->textsize(11);
        ttns_cart_setup_path->color(ttns_col_dark());
        ttns_cart_setup_path->textcolor(ttns_col_fg());
        ttns_cart_setup_path->selection_color(ttns_col_fg());
        ttns_cart_setup_path->cursor_color(ttns_col_fg());

        Fl_Button *browse = new Fl_Button(340, 28, 88, 22, "Browse...");
        browse->labelsize(11);
        ttns_theme_style_butt_button(browse, 0);
        browse->callback(ttns_cart_setup_browse_cb);

        ttns_lbl(12, 62, 80, "Playback");
        ttns_cart_setup_oneshot = new Fl_Round_Button(12, 78, 120, 20, "One-shot");
        ttns_cart_setup_oneshot->type(FL_RADIO_BUTTON);
        ttns_cart_setup_oneshot->labelsize(11);
        ttns_cart_setup_oneshot->color(ttns_col_bg());
        ttns_cart_setup_oneshot->labelcolor(ttns_col_fg());
        ttns_cart_setup_oneshot->selection_color(ttns_col_fg());
        ttns_cart_setup_oneshot->tooltip("Press again while playing to stop with ~0.3s fade-out");
        ttns_cart_setup_loop = new Fl_Round_Button(140, 78, 140, 20, "Loop (latch)");
        ttns_cart_setup_loop->type(FL_RADIO_BUTTON);
        ttns_cart_setup_loop->labelsize(11);
        ttns_cart_setup_loop->color(ttns_col_bg());
        ttns_cart_setup_loop->labelcolor(ttns_col_fg());
        ttns_cart_setup_loop->selection_color(ttns_col_fg());
        ttns_cart_setup_loop->tooltip("Press again while playing to stop with ~0.3s fade-out");

        ttns_lbl(12, 108, 48, "Level");
        ttns_cart_setup_gain = new Fl_Ttns_Fader(60, 106, 280, 22, 0, 0);
        ttns_style_slider(ttns_cart_setup_gain);
        ttns_cart_setup_gain->tooltip("Per-cart trim (dB); drag to hear live preview");
        ttns_cart_setup_gain->callback(ttns_cart_setup_gain_cb);

        Fl_Button *ok = new Fl_Button(250, 168, 80, 26, "OK");
        ttns_theme_style_butt_button(ok, 1);
        ok->callback(ttns_cart_setup_ok_cb);
        Fl_Button *cancel = new Fl_Button(340, 168, 88, 26, "Cancel");
        ttns_theme_style_butt_button(cancel, 0);
        cancel->callback(ttns_cart_setup_cancel_cb);

        ttns_cart_setup_win->end();
    }

    ttns_cart_setup_slot = slot;
    ttns_cart_setup_gain_undo = cfg.ttns.cart_slot_gain[slot];
    snprintf(title, sizeof(title), "Cart %d setup", slot + 1);
    ttns_cart_setup_win->label(title);

    if (cfg.ttns.cart_path[slot] && cfg.ttns.cart_path[slot][0])
        ttns_cart_setup_path->value(cfg.ttns.cart_path[slot]);
    else
        ttns_cart_setup_path->value("");

    if (cfg.ttns.cart_mode[slot] == TTNS_CART_LOOP)
    {
        ttns_cart_setup_oneshot->value(0);
        ttns_cart_setup_loop->value(1);
    }
    else
    {
        ttns_cart_setup_loop->value(0);
        ttns_cart_setup_oneshot->value(1);
    }
    if (ttns_cart_setup_gain)
        ttns_cart_setup_gain->value(ttns_gain_to_slider_db(cfg.ttns.cart_slot_gain[slot]));

    ttns_cart_setup_win->position(
        fl_g->window_main->x() + 40,
        fl_g->window_main->y() + TTNS_CART_Y + 20);
    ttns_cart_setup_win->show();
}

static void ttns_cart_play(int slot)
{
    if (slot < 0 || slot >= TTNS_CART_SLOTS)
        return;

    if (!ttns_cart_has_audio(slot))
    {
        fl_message("Cart %d has no audio.\nRight-click to assign a file (WAV, MP3, M4A, FLAC, OGG).", slot + 1);
        return;
    }

    ttns_cart_trigger(slot);
    if (ttns_cart_btn[slot])
        ttns_cart_btn[slot]->redraw();
}

void ttns_ui_trigger_cart(int slot)
{
    ttns_cart_play(slot);
}

static int ttns_typing_in_field(void)
{
    Fl_Widget *focus = Fl::focus();

    if (!focus)
        return 0;

    return (dynamic_cast<Fl_Input_*>(focus) ||
            dynamic_cast<Fl_Text_Display*>(focus) ||
            dynamic_cast<Fl_Value_Input*>(focus)) != NULL;
}

static int ttns_global_key_handler(int e)
{
    char k;
    static int space_armed = 0;

    if (ttns_typing_in_field())
        return 0;

    if (e == FL_KEYDOWN && Fl::event_key() == ' ')
    {
        if (!space_armed)
        {
            space_armed = 1;
            ttns_mic_toggle();
        }
        return 1;
    }

    if (e == FL_KEYUP && Fl::event_key() == ' ')
    {
        space_armed = 0;
        return 1;
    }

    if (e != FL_SHORTCUT && e != FL_KEYDOWN)
        return 0;
    if (Fl::event_length() < 1)
        return 0;

    k = Fl::event_text()[0];
    if (k < '1' || k > '8')
        return 0;
    if (Fl::event_state() & (FL_CTRL | FL_ALT | FL_META))
        return 0;

    ttns_cart_play(k - '1');
    return 1;
}

static void ttns_cart_cb(Fl_Widget *w, void *slot_vp)
{
    int slot = (int)(intptr_t)slot_vp;

    if (Fl::event_button() == FL_RIGHT_MOUSE ||
        (Fl::event_state() & (FL_CTRL | FL_META)))
    {
        ttns_cart_setup_show(slot);
        return;
    }

    ttns_cart_play(slot);
    w->redraw();
}

static void ttns_reload_carts_from_cfg(void)
{
    int i;

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        ttns_cart_set_mode(i, cfg.ttns.cart_mode[i]);
        ttns_cart_set_gain(i, cfg.ttns.cart_slot_gain[i]);
        if (cfg.ttns.cart_label[i] && cfg.ttns.cart_label[i][0])
            ttns_cart_set_label(i, cfg.ttns.cart_label[i]);
        if (cfg.ttns.cart_path[i] && cfg.ttns.cart_path[i][0])
            ttns_cart_load(i, cfg.ttns.cart_path[i]);
    }
    ttns_update_cart_labels();
}

void ttns_ui_timer_tick(void)
{
    int i;
    int duck = ttns_ducking_active();
    int line_pk = 0;
    int mic_pk = 0;
    int cart_pk = 0;

    snd_recover_if_needed();

    if (ttns_seen_dev_epoch != snd_device_list_epoch())
    {
        ttns_seen_dev_epoch = snd_device_list_epoch();
        ttns_fill_cfg_audio_devices();
        update_samplerates();
        ttns_audio_mark_applied();
    }

    ttns_meters_poll(&line_pk, &mic_pk, &cart_pk);
    if (ttns_slider_line)
        ttns_slider_line->set_peak_sample(line_pk);
    if (ttns_slider_cart)
        ttns_slider_cart->set_peak_sample(cart_pk);
    if (ttns_slider_mic)
        ttns_slider_mic->set_peak_sample(mic_pk);

    if (ttns_duck_led)
    {
        Fl_Color duck_col = duck ? ttns_col_yellow() : ttns_col_dark();
        ttns_duck_led->color(duck_col);
        ttns_duck_led->selection_color(duck_col);
        ttns_duck_led->redraw();
    }

    if (ttns_core_phone)
    {
        Fl_Color phone_col;

        if (ttns_remote_any_live())
            phone_col = ttns_col_green();
        else if (ttns_core_reach_get())
            phone_col = ttns_col_yellow();
        else
            phone_col = fl_color_average(ttns_col_fg(), ttns_col_bg(), 0.35f);
        if (ttns_core_phone->labelcolor() != phone_col)
        {
            ttns_core_phone->labelcolor(phone_col);
            ttns_core_phone->redraw();
        }
    }

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        Fl_Color bg;
        Fl_Color fg;

        if (!ttns_cart_btn[i])
            continue;

        if (ttns_cart_is_playing(i))
        {
            bg = ttns_col_green();
            fg = ttns_col_bg();
        }
        else if (ttns_cart_has_audio(i))
        {
            bg = ttns_col_dark();
            fg = ttns_col_fg();
        }
        else
        {
            bg = ttns_col_bg();
            fg = ttns_col_fg();
        }

        if (ttns_cart_btn[i]->color() != bg ||
            ttns_cart_btn[i]->labelcolor() != fg)
        {
            ttns_cart_btn[i]->set_fill(bg);
            ttns_cart_btn[i]->set_text(fg);
        }
    }

    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        int st;
        const char *lbl;
        int pk;

        if (ttns_slider_remote[i])
        {
            pk = ttns_remote_uplink_peak(i);
            if (cfg.ttns.remote_mute[i] || cfg.ttns.remote_gain[i] <= 0.0f)
                pk = 0;
            else
                pk = (int)((float)pk * cfg.ttns.remote_gain[i]);
            ttns_slider_remote[i]->set_peak_sample(pk);
        }

        if (!ttns_remote_status[i])
            continue;

        st = ttns_remote_state(i);
        if (st == TTNS_REMOTE_CONNECTED)
            lbl = ttns_remote_test_tone(i) ? "test" : "live";
        else if (st == TTNS_REMOTE_WAITING)
            lbl = "wait";
        else if (st == TTNS_REMOTE_ERROR)
            lbl = "err";
        else
            lbl = "—";
        if (!ttns_remote_status[i]->label() || strcmp(ttns_remote_status[i]->label(), lbl) != 0)
        {
            ttns_remote_status[i]->copy_label(lbl);
            ttns_remote_status[i]->redraw();
            ttns_remote_style_test_button(i);
        }
    }
}

void ttns_ui_sync_from_cfg(void)
{
    if (!ttns_slider_mic || !ttns_slider_line || !ttns_slider_cart)
        return;

    ttns_slider_mic->value(ttns_gain_to_slider_db(cfg.ttns.mic_gain));
    ttns_slider_line->value(ttns_gain_to_slider_db(cfg.ttns.line_gain));
    ttns_slider_cart->value(ttns_gain_to_slider_db(cfg.ttns.cart_gain));

    if (ttns_slider_duck_gate)
        ttns_slider_duck_gate->value(ttns_thr_lin_to_db(cfg.ttns.duck_threshold));
    if (ttns_slider_duck_depth)
        ttns_slider_duck_depth->value(cfg.ttns.duck_depth_db);

    if (ttns_choice_mount)
        ttns_zones_fill_mount_choice(ttns_choice_mount);
    ttns_ui_clear_mount_confirm();

    ttns_fill_cfg_audio_devices();
    if (ttns_chk_monitor_mute)
        ttns_chk_monitor_mute->value(cfg.ttns.mic_monitor_mute ? 0 : 1);
    if (ttns_chk_monitor_master)
        ttns_chk_monitor_master->value(cfg.ttns.monitor_mute ? 1 : 0);
    if (ttns_btn_mic)
        ttns_btn_mic->value(cfg.ttns.mic_mute ? 1 : 0);
    ttns_reload_carts_from_cfg();

    if (ttns_chk_remote_accept)
        ttns_chk_remote_accept->value(cfg.ttns.remote_accept ? 1 : 0);
    if (ttns_remote_room_lbl)
    {
        char buf[64];
        const char *code = cfg.ttns.remote_room[0] ? cfg.ttns.remote_room : "------";
        snprintf(buf, sizeof(buf), "Code %s", code);
        ttns_remote_room_lbl->copy_label(buf);
    }
    {
        int r;
        for (r = 0; r < TTNS_REMOTE_SLOTS; r++)
        {
            if (ttns_slider_remote[r])
                ttns_slider_remote[r]->value(ttns_gain_to_slider_db(cfg.ttns.remote_gain[r]));
            if (ttns_chk_remote_mute[r])
                ttns_chk_remote_mute[r]->value(cfg.ttns.remote_mute[r] ? 1 : 0);
        }
    }
}

void ttns_cfg_sync_from_ui(void)
{
    int i;
    float db;

    if (ttns_slider_line)
    {
        db = (float)ttns_slider_line->value();
        cfg.ttns.line_gain = ttns_slider_db_to_gain(db);
    }
    if (ttns_slider_cart)
    {
        db = (float)ttns_slider_cart->value();
        cfg.ttns.cart_gain = ttns_slider_db_to_gain(db);
    }
    if (ttns_slider_mic)
    {
        db = (float)ttns_slider_mic->value();
        cfg.ttns.mic_gain = ttns_slider_db_to_gain(db);
    }
    if (ttns_slider_duck_gate)
        cfg.ttns.duck_threshold = ttns_thr_db_to_lin((float)ttns_slider_duck_gate->value());
    if (ttns_slider_duck_depth)
        cfg.ttns.duck_depth_db = (float)ttns_slider_duck_depth->value();
    if (ttns_choice_mount)
        ttns_zones_index_to_mount(ttns_choice_mount->value(), &cfg.ttns.zone, &cfg.ttns.slot);
    if (fl_g && fl_g->choice_cfg_dev)
        cfg.ttns.line_dev_num = fl_g->choice_cfg_dev->value();
    if (fl_g && fl_g->choice_cfg_ttns_mic)
        cfg.ttns.mic_dev_num = fl_g->choice_cfg_ttns_mic->value();
    if (fl_g && fl_g->choice_cfg_ttns_monitor_out)
        cfg.ttns.monitor_out_dev_num = fl_g->choice_cfg_ttns_monitor_out->value();
    if (ttns_chk_monitor_mute)
        cfg.ttns.mic_monitor_mute = ttns_chk_monitor_mute->value() ? 0 : 1;
    if (ttns_chk_monitor_master)
        cfg.ttns.monitor_mute = ttns_chk_monitor_master->value() ? 1 : 0;
    if (ttns_btn_mic)
        cfg.ttns.mic_mute = ttns_btn_mic->value() ? 1 : 0;

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        cfg.ttns.cart_mode[i] = ttns_cart_get_mode(i);
        cfg.ttns.cart_slot_gain[i] = ttns_cart_get_gain(i);
    }
    if (ttns_chk_remote_accept)
        cfg.ttns.remote_accept = ttns_chk_remote_accept->value() ? 1 : 0;
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (ttns_slider_remote[i])
        {
            db = (float)ttns_slider_remote[i]->value();
            cfg.ttns.remote_gain[i] = ttns_slider_db_to_gain(db);
        }
        if (ttns_chk_remote_mute[i])
            cfg.ttns.remote_mute[i] = ttns_chk_remote_mute[i]->value() ? 1 : 0;
    }
}

void ttns_ui_apply_zone_selection(void)
{
    int zone = 1;
    int slot = 1;

    if (ttns_choice_mount)
        ttns_zones_index_to_mount(ttns_choice_mount->value(), &zone, &slot);

    ttns_zones_apply(zone, slot);
}

void ttns_ui_init(flgui *g)
{
    int i;
    int n;
    int win_w;
    int val_w;
    int btn_w;
    Fl_Widget *w;
    Fl_RGB_Image *logo_img = NULL;
    Fl_Box *sep;

    if (!g || !g->window_main)
        return;

    n = g->window_main->children();
    for (i = 0; i < n; i++)
    {
        w = g->window_main->child(i);
        w->position(w->x(), w->y() + TTNS_EXTRA_H);

        {
            Fl_Box *box = dynamic_cast<Fl_Box*>(w);
            const char *lbl;

            if (!box)
                continue;
            lbl = box->label();
            if (lbl && (!strcmp(lbl, "-24dB") || !strcmp(lbl, "+24dB")))
                box->hide();
        }
    }

    g->window_main->size(TTNS_WIN_W, g->window_main->h());
    g->window_main->size(TTNS_WIN_W, g->window_main->h() + TTNS_EXTRA_H);
    /* Keep min height loose so Remotes collapse can shrink the window later. */
    g->window_main->size_range(TTNS_WIN_W, 100, TTNS_WIN_W);

    win_w = g->window_main->w();
    val_w = win_w - TTNS_VAL_X - 12;
    btn_w = (win_w - 16) / TTNS_CART_SLOTS;

    g->window_main->begin();

    logo_img = ttns_load_logo_scaled(TTNS_LOGO);
    if (logo_img)
    {
        Fl_Box *logo = new Fl_Box(6, 6, logo_img->w(), logo_img->h());
        logo->image(logo_img);
        logo->box(FL_NO_BOX);
    }
    else
    {
        Fl_Box *logo = new Fl_Box(6, 16, TTNS_LOGO, 20, "TTNS");
        logo->labelfont(FL_BOLD);
        logo->labelsize(12);
        logo->box(FL_NO_BOX);
        ttns_theme_style_label_box(logo);
    }

    {
        Fl_Button *about_btn = new Fl_Button(8, 62, 48, 18, "About");
        about_btn->labelsize(10);
        about_btn->box(FL_NO_BOX);
        ttns_theme_style_butt_button(about_btn, 0);
        about_btn->callback(ttns_about_cb);
    }

    ttns_btn_mic = new Fl_Ttns_Mic_Button(TTNS_MIC_BTN_X, TTNS_MIC_BTN_Y,
                                          TTNS_MIC_BTN_W, TTNS_MIC_BTN_H);
    ttns_btn_mic->labelsize(9);
    ttns_btn_mic->tooltip("Mic on/off air — muted when pressed (red X). Space toggles.");
    ttns_btn_mic->callback(ttns_mic_mute_cb);
    ttns_btn_mic->shortcut(0);

    /* Space under mic button; keep both above the cart row, with a clear gap between. */
    ttns_chk_monitor_mute = new Fl_Ttns_Check_Button(TTNS_MIC_BTN_X, TTNS_MIC_BTN_Y + TTNS_MIC_BTN_H + 4,
                                                     TTNS_MIC_BTN_W + 4, 14, "Mic mon");
    ttns_chk_monitor_mute->labelsize(9);
    ttns_style_check(ttns_chk_monitor_mute);
    ttns_chk_monitor_mute->indicator(TTNS_CHECK_ONOFF);
    ttns_chk_monitor_mute->tooltip("When checked: hear the local Mic in headphones.\n"
                                    "Does not mute the mic on the stream — use the mic button for that.");
    ttns_chk_monitor_mute->callback(ttns_monitor_cb);

    ttns_chk_monitor_master = new Fl_Ttns_Check_Button(TTNS_MIC_BTN_X, TTNS_MIC_BTN_Y + TTNS_MIC_BTN_H + 22,
                                                       TTNS_MIC_BTN_W + 4, 14, "Mon mute");
    ttns_chk_monitor_master->labelsize(9);
    ttns_style_check(ttns_chk_monitor_master);
    ttns_chk_monitor_master->indicator(TTNS_CHECK_NEGATE);
    ttns_chk_monitor_master->tooltip("Mute all local monitor/headphones (line, carts, mic, remotes).\n"
                                      "Icecast/stream output is unaffected — useful with an external mixer.");
    ttns_chk_monitor_master->callback(ttns_monitor_master_cb);

    ttns_duck_led = new Fl_Box(win_w - 24, 10, 18, 18);
    ttns_duck_led->box(FL_ROUND_UP_BOX);
    ttns_duck_led->color(ttns_col_dark());
    ttns_duck_led->selection_color(ttns_col_dark());
    ttns_duck_led->tooltip("Yellow = line+carts ducked while mic is above gate level\n(requires separate Deck and Mic devices)");

    ttns_duck_lbl = ttns_lbl(win_w - 76, 10, 36, "Duck");

    ttns_lbl(TTNS_LBL_X, 10, 48, "Mount");
    ttns_choice_mount = new Fl_Choice(TTNS_VAL_X, 8, TTNS_ZONE_LIST_W, 24);
    ttns_style_choice(ttns_choice_mount);

    /* Failsafe immediately after Mount — compact so the tick sits with the mount, not Duck. */
    {
        const int conf_gap = 6;
        const int conf_w = 78;
        int conf_x = TTNS_VAL_X + TTNS_ZONE_LIST_W + conf_gap;
        ttns_chk_mount_confirm = new Fl_Ttns_Check_Button(conf_x, 8, conf_w, 24, "Confirm");
        ttns_style_check(ttns_chk_mount_confirm);
        ttns_chk_mount_confirm->indicator(TTNS_CHECK_AFFIRM);
        ttns_chk_mount_confirm->value(0);
        ttns_chk_mount_confirm->callback(ttns_mount_confirm_cb);
        ttns_chk_mount_confirm->tooltip(
            "Tick to confirm this Mount before going live.\n"
            "Failsafe: stops accidental connect to zone 1-1 (or any zone)\n"
            "from cutting over another DJ. Clears when you change Mount.\n"
            "Play stays disabled until Confirm is ticked.");
    }

    ttns_lbl(TTNS_LBL_X, 42, 38, "Line");
    ttns_slider_line = new Fl_Ttns_Fader(TTNS_VAL_X, 40, val_w, TTNS_FADER_H);
    ttns_style_slider(ttns_slider_line);
    ttns_slider_line->callback(ttns_gain_cb, (void*)(intptr_t)1);

    ttns_lbl(TTNS_LBL_X, 68, 38, "Cart");
    ttns_slider_cart = new Fl_Ttns_Fader(TTNS_VAL_X, 66, val_w, TTNS_FADER_H);
    ttns_style_slider(ttns_slider_cart);
    ttns_slider_cart->callback(ttns_gain_cb, (void*)(intptr_t)2);
    ttns_slider_cart->tooltip("Master level for all cart slots (still ducked with line)");

    ttns_lbl(TTNS_LBL_X, 94, 38, "Mic");
    ttns_slider_mic = new Fl_Ttns_Fader(TTNS_VAL_X, 92, val_w, TTNS_FADER_H);
    ttns_style_slider(ttns_slider_mic);
    ttns_slider_mic->callback(ttns_gain_cb, (void*)(intptr_t)0);

    ttns_lbl(TTNS_LBL_X, 120, 38, "Gate");
    ttns_slider_duck_gate = new Fl_Ttns_Fader(TTNS_VAL_X, 118, val_w, TTNS_FADER_H, 0, 0);
    ttns_style_duck_slider(ttns_slider_duck_gate, -50.0, -6.0);
    ttns_slider_duck_gate->callback(ttns_duck_gate_cb);
    ttns_slider_duck_gate->tooltip("Mic level (dB) that triggers ducking of line+carts");

    ttns_lbl(TTNS_LBL_X, 144, 38, "Depth");
    ttns_slider_duck_depth = new Fl_Ttns_Fader(TTNS_VAL_X, 142, val_w, TTNS_FADER_H, 0, 0);
    ttns_style_duck_slider(ttns_slider_duck_depth, -24.0, 0.0);
    ttns_slider_duck_depth->callback(ttns_duck_depth_cb);
    ttns_slider_duck_depth->tooltip("How far line+carts drop while ducked (dB)");

    sep = new Fl_Box(8, TTNS_CART_Y - 2, win_w - 16, 2);
    sep->box(FL_BORDER_FRAME);
    sep->color(ttns_col_fg());

    for (i = 0; i < TTNS_CART_SLOTS; i++)
    {
        char lblbuf[8];
        snprintf(lblbuf, sizeof(lblbuf), "%d", i + 1);
        ttns_cart_btn[i] = new Fl_Ttns_Cart_Button(8 + i * btn_w, TTNS_CART_Y + 4,
                                                    btn_w - 2, 32, lblbuf);
        ttns_cart_btn[i]->labelsize(12);
        ttns_cart_btn[i]->shortcut((char)('1' + i));
        ttns_cart_btn[i]->tooltip("Play (keys 1-8, right-click or Ctrl+click to setup)");
        ttns_cart_btn[i]->callback(ttns_cart_cb, (void*)(intptr_t)i);
    }

    {
        int mute_w = 48;
        int test_w = 28;
        int status_w = 36;
        int fad_x = 8 + mute_w + 4;
        int fad_w = win_w - fad_x - status_w - test_w - 14;

        /*
         * Create off to the side; ttns_remote_relayout() parks them under the
         * LCD after the deck geometry is known (More-panel style).
         */
        ttns_btn_remote_toggle = new Fl_Ttns_Border_Button(8, 0, 88, 24, "Remotes@>");
        ttns_btn_remote_toggle->labelsize(11);
        ttns_btn_remote_toggle->labelfont(FL_BOLD);
        ttns_btn_remote_toggle->tooltip("Show or hide remote co-host channels");
        ttns_btn_remote_toggle->callback(ttns_remote_toggle_cb);

        ttns_chk_remote_accept = new Fl_Ttns_Check_Button(100, 0, 72, 24, "Accept");
        ttns_style_check(ttns_chk_remote_accept);
        ttns_chk_remote_accept->indicator(TTNS_CHECK_AFFIRM);
        ttns_chk_remote_accept->tooltip("Allow up to 4 remote co-hosts to join with the room code");
        ttns_chk_remote_accept->callback(ttns_remote_accept_cb);

        ttns_chk_ptt_remotes = new Fl_Ttns_Check_Button(200, 0, 50, 24, "PTT");
        ttns_style_check(ttns_chk_ptt_remotes);
        ttns_chk_ptt_remotes->indicator(TTNS_CHECK_AFFIRM);
        ttns_chk_ptt_remotes->tooltip("Talk to remotes only — host mic and remotes stay off-air; program is not ducked. Click again to put voices back on air.");
        ttns_chk_ptt_remotes->callback(ttns_ptt_remotes_cb);

        /* Telephone LED: grey=core unreachable, yellow=reachable, green=remote live. */
        ttns_core_phone = new Fl_Box(174, 2, 22, 20, "\xE2\x98\x8E"); /* ☎ */
        ttns_core_phone->box(FL_NO_BOX);
        ttns_core_phone->labelsize(16);
        ttns_core_phone->labelfont(FL_HELVETICA);
        ttns_core_phone->labelcolor(ttns_col_dark());
        ttns_core_phone->tooltip("core.liveencode.com — grey offline, yellow reachable, green remotes active");
        ttns_core_phone->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

        ttns_remote_room_lbl = new Fl_Box(178, 0, 140, 24, "Code ------");
        ttns_remote_room_lbl->labelsize(11);
        ttns_remote_room_lbl->labelfont(FL_BOLD);
        ttns_remote_room_lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ttns_remote_room_lbl->box(FL_NO_BOX);
        ttns_theme_style_label_box(ttns_remote_room_lbl);

        ttns_btn_remote_newcode = new Fl_Ttns_Border_Button(win_w - 86, 0, 78, 24, "New code");
        ttns_btn_remote_newcode->labelsize(10);
        ttns_btn_remote_newcode->callback(ttns_remote_newcode_cb);
        ttns_btn_remote_newcode->tooltip("Generate a new room code (invalidates the previous one)");

        for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
        {
            char lblbuf[8];

            snprintf(lblbuf, sizeof(lblbuf), "R%d", i + 1);
            ttns_chk_remote_mute[i] = new Fl_Ttns_Check_Button(8, 0, mute_w, 22, NULL);
            ttns_chk_remote_mute[i]->copy_label(lblbuf);
            ttns_style_check(ttns_chk_remote_mute[i]);
            ttns_chk_remote_mute[i]->indicator(TTNS_CHECK_NEGATE);
            ttns_chk_remote_mute[i]->tooltip("Mute this remote in the program mix (checked = muted)");
            ttns_chk_remote_mute[i]->callback(ttns_remote_mute_cb, (void*)(intptr_t)i);
            ttns_chk_remote_mute[i]->hide();

            ttns_slider_remote[i] = new Fl_Ttns_Fader(fad_x, 0, fad_w, TTNS_FADER_H);
            ttns_style_slider(ttns_slider_remote[i]);
            ttns_slider_remote[i]->callback(ttns_remote_gain_cb, (void*)(intptr_t)i);
            ttns_slider_remote[i]->tooltip("Remote co-host level in the program mix");
            ttns_slider_remote[i]->hide();

            ttns_btn_remote_test[i] = new Fl_Button(fad_x + fad_w + 2, 0, test_w, 22, "T");
            ttns_btn_remote_test[i]->labelsize(10);
            ttns_theme_style_butt_button(ttns_btn_remote_test[i], 0);
            ttns_btn_remote_test[i]->tooltip("Inject a local test tone into this slot (no network)");
            ttns_btn_remote_test[i]->callback(ttns_remote_test_cb, (void*)(intptr_t)i);
            ttns_btn_remote_test[i]->hide();

            ttns_remote_status[i] = new Fl_Box(fad_x + fad_w + test_w + 4, 0, status_w, 22, NULL);
            ttns_remote_status[i]->copy_label("-");
            ttns_remote_status[i]->labelsize(10);
            ttns_remote_status[i]->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
            ttns_remote_status[i]->box(FL_NO_BOX);
            ttns_theme_style_label_box(ttns_remote_status[i]);
            ttns_remote_status[i]->hide();
        }
    }

    ttns_choice_mount->callback(ttns_zone_cb);

    if (fl_g->choice_cfg_dev)
        fl_g->choice_cfg_dev->callback(ttns_dev_cb, NULL);
    if (fl_g->choice_cfg_ttns_mic)
        fl_g->choice_cfg_ttns_mic->callback(ttns_dev_cb, NULL);
    if (fl_g->choice_cfg_ttns_monitor_out)
        fl_g->choice_cfg_ttns_monitor_out->callback(ttns_dev_cb, NULL);

    if (fl_g->choice_cfg_dev && fl_g->choice_cfg_dev->parent())
    {
        Fl_Group *ag = (Fl_Group *)fl_g->choice_cfg_dev->parent();
        ag->begin();
        ttns_btn_refresh_dev = new Fl_Button(23, 214, 250, 16, "Refresh devices");
        ttns_btn_refresh_dev->labelsize(10);
        ttns_btn_refresh_dev->tooltip("Rescan USB/virtual audio devices. Causes a short gap if you are on air.");
        ttns_theme_style_butt_button(ttns_btn_refresh_dev, 0);
        ttns_btn_refresh_dev->callback(ttns_refresh_dev_cb);
        ag->end();
    }

    Fl::add_handler(ttns_global_key_handler);

    g->window_main->end();

    g->slider_gain->hide();
    g->button_cfg->copy_label("Settings@>");
    g->window_cfg->label("TTNS Advanced");
    g->window_main->label("TTNS Deck");
    ttns_set_window_icon(g->window_main);

    ttns_zones_load();
    ttns_ui_sync_from_cfg();
    ttns_ui_apply_zone_selection();
    ttns_ui_sync_advanced();
    ttns_theme_apply(g);
    ttns_layout_feedback_panel();
    ttns_raise_duck_led(g);
    /* Park Remotes under the LCD, then clip like More. */
    ttns_core_reach_start();
    ttns_remote_relayout();
    info_panel_collapse();
    ttns_remote_sync_expand();
    ttns_ui_timer_tick();
    ttns_ui_update_connect_armed();

    g->window_main->redraw();
}
