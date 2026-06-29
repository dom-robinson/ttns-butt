#include "FL/Fl_Ttns_Fader.H"

#include <math.h>
#include <sys/time.h>

#include <FL/fl_draw.H>

#include "ttns_theme.h"
#include "vu_meter.h"

static double ttns_wall_sec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static float ttns_fader_sample_to_level(int sample)
{
    float db;

    if (sample <= 0)
        return 0.0f;

    db = -(20.0f * (float)log10(32768.0f / (float)sample)) + (float)VU_OFFSET;
    if (db < -50.0f)
        db = -50.0f;
    if (db > 6.0f)
        db = 6.0f;

    return (db + 50.0f) / 56.0f;
}

Fl_Ttns_Fader::Fl_Ttns_Fader(int x, int y, int w, int h, const char *l, int meter_enabled)
    : Fl_My_Value_Slider(x, y, w, h, l)
{
    level_ = 0.0f;
    peak_ = 0.0f;
    peak_until_sec_ = 0.0;
    meter_enabled_ = meter_enabled ? 1 : 0;
    box(FL_NO_BOX);
    slider_size(0.14);
    color(ttns_col_bg());
    selection_color(ttns_col_fg());
}

void Fl_Ttns_Fader::set_peak_sample(int sample16)
{
    float target = ttns_fader_sample_to_level(sample16);
    double now = ttns_wall_sec();

    level_ = level_ * 0.55f + target * 0.45f;

    if (target >= peak_ - 0.002f)
    {
        peak_ = target;
        peak_until_sec_ = now + 0.85;
    }
    else if (now > peak_until_sec_)
        peak_ *= 0.88f;

    if (peak_ < level_)
        peak_ = level_;

    redraw();
}

int Fl_Ttns_Fader::handle(int e)
{
    int ret;

    ret = Fl_My_Value_Slider::handle(e);

    if (e == FL_PUSH || e == FL_DRAG || e == FL_RELEASE)
    {
        damage(FL_DAMAGE_ALL);
        if (window())
            window()->damage(FL_DAMAGE_EXPOSE);
    }

    return ret;
}

static void ttns_draw_fader_handle(Fl_Slider *s)
{
    int sw = s->w();
    int sh = s->h();
    int sz = (int)(s->slider_size() * ((sw < sh) ? sw : sh));
    double t;

    if (sz < 7)
        sz = 7;

    if (s->maximum() > s->minimum())
        t = (s->value() - s->minimum()) / (s->maximum() - s->minimum());
    else
        t = 0.0;

    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;

    {
        int ix = s->x() + (int)(t * (double)(sw - sz));
        int iy = s->y() + (sh - sz) / 2;

        fl_color(ttns_col_red());
        fl_rectf(ix, iy, sz, sz);
        fl_color(fl_color_average(ttns_col_red(), FL_WHITE, 0.25f));
        fl_rect(ix, iy, sz, sz);
    }
}

static void ttns_draw_meter_segment(int x, int y, int w, int h, int x0, int x1, Fl_Color c)
{
    int lw;

    if (x1 <= x0 || w <= 0)
        return;

    lw = x1 - x0;
    if (lw < 1)
        lw = 1;

    fl_color(c);
    fl_rectf(x + x0, y + 1, lw, h - 2);
}

static void ttns_draw_meter_bar(int x, int y, int w, int h, float level, float peak)
{
    int fill_w, peak_x;
    int g_end, o_end;

    fl_color(ttns_col_dark());
    fl_rectf(x, y, w, h);

    fill_w = (int)(level * (float)w);
    if (fill_w > w)
        fill_w = w;

    g_end = (int)((float)w * (42.0f / 56.0f));
    o_end = (int)((float)w * (46.0f / 56.0f));

    if (fill_w > 0)
    {
        int seg;

        seg = fill_w;
        if (seg > g_end)
            seg = g_end;
        ttns_draw_meter_segment(x, y, w, h, 0, seg, ttns_col_green());

        if (fill_w > g_end)
        {
            seg = fill_w;
            if (seg > o_end)
                seg = o_end;
            ttns_draw_meter_segment(x, y, w, h, g_end, seg, ttns_col_orange());
        }

        if (fill_w > o_end)
            ttns_draw_meter_segment(x, y, w, h, o_end, fill_w, ttns_col_meter_red());
    }

    peak_x = (int)(peak * (float)w);
    if (peak_x > 0 && peak_x < w)
    {
        fl_color(fl_color_average(ttns_col_fg(), FL_WHITE, 0.35f));
        fl_rectf(x + peak_x, y + 1, 2, h - 2);
    }
}

void Fl_Ttns_Fader::draw(void)
{
    int tx, ty;

    fl_color(ttns_col_bg());
    fl_rectf(x(), y(), w(), h());

    tx = x();
    ty = y() + (h() - TRACK_H) / 2;

    if (meter_enabled_)
        ttns_draw_meter_bar(tx, ty, w(), TRACK_H, level_, peak_);
    else
    {
        fl_color(ttns_col_dark());
        fl_rectf(tx, ty, w(), TRACK_H);
    }

    ttns_draw_fader_handle(this);
}
