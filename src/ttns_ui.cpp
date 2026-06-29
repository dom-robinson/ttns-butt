#include "ttns_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

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

#include "FL/Fl_My_Native_File_Chooser.H"
#include "FL/Fl_My_Value_Slider.H"
#include "FL/Fl_Ttns_Mic_Button.H"
#include "FL/Fl_Ttns_Check_Button.H"
#include "FL/Fl_Ttns_Cart_Button.H"
#include "FL/Fl_Ttns_Fader.H"
#include "cart_player.h"
#include "cfg.h"
#include "fl_callbacks.h"
#include "fl_funcs.h"
#include "flgui.h"
#include "port_audio.h"
#include "ttns_audio.h"
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
static const int TTNS_ZONE_LIST_W = 220;
static const int TTNS_FADER_H = 22;
static const int TTNS_CART_Y = 168;
static const int TTNS_CART_H = 38;
static const int TTNS_EXTRA_H = TTNS_CART_Y + TTNS_CART_H;

static const int TTNS_MIC_BTN_X = 6;
static const int TTNS_MIC_BTN_Y = 84;
static const int TTNS_MIC_BTN_W = 58;
static const int TTNS_MIC_BTN_H = 56;

static Fl_Choice *ttns_choice_mount = NULL;
static Fl_Box *ttns_duck_lbl = NULL;
static Fl_Ttns_Fader *ttns_slider_mic = NULL;
static Fl_Ttns_Fader *ttns_slider_line = NULL;
static Fl_Ttns_Fader *ttns_slider_duck_gate = NULL;
static Fl_Ttns_Fader *ttns_slider_duck_depth = NULL;
static Fl_Box *ttns_duck_led = NULL;
static Fl_Ttns_Check_Button *ttns_chk_monitor = NULL;
static Fl_Ttns_Check_Button *ttns_chk_monitor_mute = NULL;
static Fl_Ttns_Mic_Button *ttns_btn_mic = NULL;
static Fl_Ttns_Cart_Button *ttns_cart_btn[TTNS_CART_SLOTS];

static Fl_Window *ttns_cart_setup_win = NULL;
static Fl_Input *ttns_cart_setup_path = NULL;
static Fl_Round_Button *ttns_cart_setup_oneshot = NULL;
static Fl_Round_Button *ttns_cart_setup_loop = NULL;
static int ttns_cart_setup_slot = 0;

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

static void ttns_reopen_audio(void)
{
    snd_reinit();
    ttns_mixer_reset();
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
        update_samplerates();
    }
    else if (w == (Fl_Widget*)fl_g->choice_cfg_ttns_mic)
        cfg.ttns.mic_dev_num = fl_g->choice_cfg_ttns_mic->value();

    unsaved_changes = 1;
    ttns_reopen_audio();
}

static void ttns_gain_cb(Fl_Widget *w, void *which)
{
    float db = (float)((Fl_My_Value_Slider*)w)->value();
    float factor = ((int)db == 0) ? 1.0f : util_db_to_factor(db);

    if ((int)(intptr_t)which)
        cfg.ttns.line_gain = factor;
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
    cfg.ttns.mic_monitor = ttns_chk_monitor ? ttns_chk_monitor->value() : 0;
    cfg.ttns.mic_monitor_mute = ttns_chk_monitor_mute ? ttns_chk_monitor_mute->value() : 0;
    unsaved_changes = 1;
    ttns_reopen_audio();
}

static void ttns_zone_cb(Fl_Widget *, void *)
{
    ttns_ui_apply_zone_selection();
    ttns_ui_sync_advanced();
    unsaved_changes = 1;
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
        fl_alert("Could not load audio:\n%s\n(Supported: WAV, MP3, FLAC, OGG)", picked);
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

static void ttns_cart_setup_browse_cb(Fl_Widget *, void *)
{
    Fl_My_Native_File_Chooser nfc;
    const char *picked;

    nfc.title("Select cart audio");
    nfc.type(Fl_My_Native_File_Chooser::BROWSE_FILE);
    nfc.filter("Audio Files\t*.wav;*.mp3;*.flac;*.ogg");
    if (nfc.show() != 0)
        return;

    picked = nfc.filename();
    if (!picked || !picked[0])
        return;

    ttns_cart_setup_path->value(picked);
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
    ttns_cart_setup_win->hide();
}

static void ttns_cart_setup_show(int slot)
{
    char title[32];

    if (!ttns_cart_setup_win)
    {
        ttns_cart_setup_win = new Fl_Window(440, 190, "Cart setup");
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
        ttns_cart_setup_oneshot->labelsize(11);
        ttns_cart_setup_oneshot->color(ttns_col_bg());
        ttns_cart_setup_oneshot->labelcolor(ttns_col_fg());
        ttns_cart_setup_oneshot->selection_color(ttns_col_fg());
        ttns_cart_setup_loop = new Fl_Round_Button(140, 78, 140, 20, "Loop (latch)");
        ttns_cart_setup_loop->labelsize(11);
        ttns_cart_setup_loop->color(ttns_col_bg());
        ttns_cart_setup_loop->labelcolor(ttns_col_fg());
        ttns_cart_setup_loop->selection_color(ttns_col_fg());
        ttns_cart_setup_loop->tooltip("Press again while playing to stop with fade-out");

        Fl_Button *ok = new Fl_Button(250, 148, 80, 26, "OK");
        ttns_theme_style_butt_button(ok, 1);
        ok->callback(ttns_cart_setup_ok_cb);
        Fl_Button *cancel = new Fl_Button(340, 148, 88, 26, "Cancel");
        ttns_theme_style_butt_button(cancel, 0);
        cancel->callback(ttns_cart_setup_cancel_cb);

        ttns_cart_setup_win->end();
    }

    ttns_cart_setup_slot = slot;
    snprintf(title, sizeof(title), "Cart %d setup", slot + 1);
    ttns_cart_setup_win->label(title);

    if (cfg.ttns.cart_path[slot] && cfg.ttns.cart_path[slot][0])
        ttns_cart_setup_path->value(cfg.ttns.cart_path[slot]);
    else
        ttns_cart_setup_path->value("");

    if (cfg.ttns.cart_mode[slot] == TTNS_CART_LOOP)
        ttns_cart_setup_loop->value(1);
    else
        ttns_cart_setup_oneshot->value(1);

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
        fl_message("Cart %d has no audio.\nRight-click to assign a file (WAV, MP3, FLAC, OGG).", slot + 1);
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

    ttns_meters_poll(&line_pk, &mic_pk);
    if (ttns_slider_line)
        ttns_slider_line->set_peak_sample(line_pk);
    if (ttns_slider_mic)
        ttns_slider_mic->set_peak_sample(mic_pk);

    if (ttns_duck_led)
    {
        Fl_Color duck_col = duck ? ttns_col_yellow() : ttns_col_dark();
        ttns_duck_led->color(duck_col);
        ttns_duck_led->selection_color(duck_col);
        ttns_duck_led->redraw();
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
}

void ttns_ui_sync_from_cfg(void)
{
    if (!ttns_slider_mic || !ttns_slider_line)
        return;

    ttns_slider_mic->value(util_factor_to_db(cfg.ttns.mic_gain));
    ttns_slider_line->value(util_factor_to_db(cfg.ttns.line_gain));

    if (ttns_slider_duck_gate)
        ttns_slider_duck_gate->value(ttns_thr_lin_to_db(cfg.ttns.duck_threshold));
    if (ttns_slider_duck_depth)
        ttns_slider_duck_depth->value(cfg.ttns.duck_depth_db);

    if (ttns_choice_mount)
        ttns_zones_fill_mount_choice(ttns_choice_mount);

    ttns_fill_cfg_audio_devices();
    if (ttns_chk_monitor)
        ttns_chk_monitor->value(cfg.ttns.mic_monitor);
    if (ttns_chk_monitor_mute)
        ttns_chk_monitor_mute->value(cfg.ttns.mic_monitor_mute);
    if (ttns_btn_mic)
        ttns_btn_mic->value(cfg.ttns.mic_mute ? 1 : 0);
    ttns_reload_carts_from_cfg();
}

void ttns_cfg_sync_from_ui(void)
{
    int i;
    float db;

    if (ttns_slider_line)
    {
        db = (float)ttns_slider_line->value();
        cfg.ttns.line_gain = ((int)db == 0) ? 1.0f : util_db_to_factor(db);
    }
    if (ttns_slider_mic)
    {
        db = (float)ttns_slider_mic->value();
        cfg.ttns.mic_gain = ((int)db == 0) ? 1.0f : util_db_to_factor(db);
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
    if (ttns_chk_monitor)
        cfg.ttns.mic_monitor = ttns_chk_monitor->value();
    if (ttns_chk_monitor_mute)
        cfg.ttns.mic_monitor_mute = ttns_chk_monitor_mute->value();
    if (ttns_btn_mic)
        cfg.ttns.mic_mute = ttns_btn_mic->value() ? 1 : 0;

    for (i = 0; i < TTNS_CART_SLOTS; i++)
        cfg.ttns.cart_mode[i] = ttns_cart_get_mode(i);
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
    g->window_main->size_range(TTNS_WIN_W, g->window_main->h(), TTNS_WIN_W);

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
    ttns_btn_mic->tooltip("Toggle mic on/off air (Space)");
    ttns_btn_mic->callback(ttns_mic_mute_cb);
    ttns_btn_mic->shortcut(0);

    ttns_duck_led = new Fl_Box(win_w - 24, 10, 18, 18);
    ttns_duck_led->box(FL_ROUND_UP_BOX);
    ttns_duck_led->color(ttns_col_dark());
    ttns_duck_led->selection_color(ttns_col_dark());
    ttns_duck_led->tooltip("Yellow = line+carts ducked while mic is above gate level\n(requires separate Deck and Mic devices)");

    ttns_duck_lbl = ttns_lbl(win_w - 76, 10, 36, "Duck");

    ttns_lbl(TTNS_LBL_X, 10, 48, "Mount");
    ttns_choice_mount = new Fl_Choice(TTNS_VAL_X, 8, TTNS_ZONE_LIST_W, 24);
    ttns_style_choice(ttns_choice_mount);

    ttns_lbl(TTNS_LBL_X, 42, 38, "Line");
    ttns_slider_line = new Fl_Ttns_Fader(TTNS_VAL_X, 40, val_w, TTNS_FADER_H);
    ttns_style_slider(ttns_slider_line);
    ttns_slider_line->callback(ttns_gain_cb, (void*)(intptr_t)1);

    ttns_lbl(TTNS_LBL_X, 68, 38, "Mic");
    ttns_slider_mic = new Fl_Ttns_Fader(TTNS_VAL_X, 66, val_w, TTNS_FADER_H);
    ttns_style_slider(ttns_slider_mic);
    ttns_slider_mic->callback(ttns_gain_cb, (void*)(intptr_t)0);

    ttns_lbl(TTNS_LBL_X, 94, 38, "Gate");
    ttns_slider_duck_gate = new Fl_Ttns_Fader(TTNS_VAL_X, 92, val_w, TTNS_FADER_H, 0, 0);
    ttns_style_duck_slider(ttns_slider_duck_gate, -50.0, -6.0);
    ttns_slider_duck_gate->callback(ttns_duck_gate_cb);
    ttns_slider_duck_gate->tooltip("Mic level (dB) that triggers ducking of line+carts");

    ttns_lbl(TTNS_LBL_X, 118, 38, "Depth");
    ttns_slider_duck_depth = new Fl_Ttns_Fader(TTNS_VAL_X, 116, val_w, TTNS_FADER_H, 0, 0);
    ttns_style_duck_slider(ttns_slider_duck_depth, -24.0, 0.0);
    ttns_slider_duck_depth->callback(ttns_duck_depth_cb);
    ttns_slider_duck_depth->tooltip("How far line+carts drop while ducked (dB)");

    {
        int row_y = 142;
        int row_r = TTNS_VAL_X + val_w;
        const int mic_mon_w = 98;
        const int monitor_w = 78;
        const int row_gap = 12;

        ttns_chk_monitor_mute = new Fl_Ttns_Check_Button(row_r - mic_mon_w, row_y,
                                                         mic_mon_w, 22, "Mic to Mon");
        ttns_chk_monitor_mute->labelsize(11);
        ttns_style_check(ttns_chk_monitor_mute);
        ttns_chk_monitor_mute->tooltip("Silence mic monitor without affecting stream");
        ttns_chk_monitor_mute->callback(ttns_monitor_cb);

        ttns_chk_monitor = new Fl_Ttns_Check_Button(row_r - mic_mon_w - row_gap - monitor_w,
                                                    row_y, monitor_w, 22, "Monitor");
        ttns_chk_monitor->labelsize(11);
        ttns_style_check(ttns_chk_monitor);
        ttns_chk_monitor->tooltip("Route mic to headphones (separate mic device required)");
        ttns_chk_monitor->callback(ttns_monitor_cb);
    }

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

    ttns_choice_mount->callback(ttns_zone_cb);

    if (fl_g->choice_cfg_dev)
        fl_g->choice_cfg_dev->callback(ttns_dev_cb, NULL);
    if (fl_g->choice_cfg_ttns_mic)
        fl_g->choice_cfg_ttns_mic->callback(ttns_dev_cb, NULL);

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
    ttns_ui_timer_tick();
    info_panel_collapse();
    g->window_main->size_range(TTNS_WIN_W, g->window_main->h(), TTNS_WIN_W);

    g->window_main->redraw();
}
