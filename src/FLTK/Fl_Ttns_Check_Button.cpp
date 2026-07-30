#include "FL/Fl_Ttns_Check_Button.H"

#include <FL/fl_draw.H>

/* Match ttns_theme palette (keep Remote free of Deck theme deps). */
static Fl_Color chk_bg(void)    { return fl_rgb_color(0, 0, 0); }
static Fl_Color chk_red(void)   { return fl_rgb_color(227, 27, 35); }
static Fl_Color chk_green(void) { return fl_rgb_color(51, 255, 51); }
static Fl_Color chk_fg(void)    { return fl_rgb_color(227, 27, 35); }

Fl_Ttns_Check_Button::Fl_Ttns_Check_Button(int x, int y, int w, int h, const char *l)
    : Fl_Check_Button(x, y, w, h, l), indicator_(TTNS_CHECK_AFFIRM)
{
    box(FL_NO_BOX);
    down_box(FL_NO_BOX);
    color(chk_bg());
    selection_color(chk_red());
    labelcolor(chk_fg());
}

void Fl_Ttns_Check_Button::indicator(int mode)
{
    if (mode == TTNS_CHECK_NEGATE)
        indicator_ = TTNS_CHECK_NEGATE;
    else if (mode == TTNS_CHECK_ONOFF)
        indicator_ = TTNS_CHECK_ONOFF;
    else
        indicator_ = TTNS_CHECK_AFFIRM;
}

static void chk_draw_x(int sx, int sy, int ss, int pad, Fl_Color col)
{
    fl_color(col);
    fl_line_style(FL_SOLID, 2);
    fl_line(sx + pad, sy + pad, sx + ss - pad - 1, sy + ss - pad - 1);
    fl_line(sx + ss - pad - 1, sy + pad, sx + pad, sy + ss - pad - 1);
}

static void chk_draw_check(int sx, int sy, int ss, int pad, Fl_Color col)
{
    fl_color(col);
    fl_line_style(FL_SOLID, 2);
    fl_line(sx + pad, sy + ss / 2, sx + ss / 2 - 1, sy + ss - pad - 1);
    fl_line(sx + ss / 2 - 1, sy + ss - pad - 1, sx + ss - pad - 1, sy + pad);
}

void Fl_Ttns_Check_Button::draw(void)
{
    int ss = 14;
    int sx = x() + w() - ss - 2;
    int sy = y() + (h() - ss) / 2;
    int cx = sx + ss / 2;
    int cy = sy + ss / 2;
    int r = ss / 2 - 1;
    int pad = 3;
    Fl_Color border = chk_red();
    Fl_Color bg = chk_bg();
    int on = value() ? 1 : 0;

    if (!active())
    {
        border = fl_color_average(border, chk_bg(), 0.5f);
        bg = fl_color_average(bg, chk_bg(), 0.55f);
    }

    fl_color(bg);
    fl_pie(cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0.0, 360.0);
    fl_color(border);
    fl_line_style(FL_SOLID, 2);
    fl_arc(cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0.0, 360.0);

    if (indicator_ == TTNS_CHECK_ONOFF)
    {
        if (on)
            chk_draw_check(sx, sy, ss, pad, active() ? chk_green() : border);
        else
            chk_draw_x(sx, sy, ss, pad, active() ? chk_red() : border);
    }
    else if (on)
    {
        if (indicator_ == TTNS_CHECK_NEGATE)
            chk_draw_x(sx, sy, ss, pad, active() ? chk_red() : border);
        else
            chk_draw_check(sx, sy, ss, pad, active() ? chk_green() : border);
    }

    fl_line_style(0);

    if (damage() & FL_DAMAGE_ALL)
        draw_box();

    draw_label(x(), y(), sx - x() - 4, h(), FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
}
