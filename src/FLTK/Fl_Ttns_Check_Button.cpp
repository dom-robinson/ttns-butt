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
    indicator_ = (mode == TTNS_CHECK_NEGATE) ? TTNS_CHECK_NEGATE : TTNS_CHECK_AFFIRM;
}

void Fl_Ttns_Check_Button::draw(void)
{
    int ss = 14;
    int sx = x() + w() - ss - 2;
    int sy = y() + (h() - ss) / 2;
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
    fl_rectf(sx, sy, ss, ss);
    fl_color(border);
    fl_line_style(FL_SOLID, 2);
    fl_rect(sx, sy, ss, ss);

    if (on)
    {
        if (indicator_ == TTNS_CHECK_NEGATE)
        {
            fl_color(active() ? chk_red() : border);
            fl_line_style(FL_SOLID, 2);
            fl_line(sx + pad, sy + pad, sx + ss - pad - 1, sy + ss - pad - 1);
            fl_line(sx + ss - pad - 1, sy + pad, sx + pad, sy + ss - pad - 1);
        }
        else
        {
            fl_color(active() ? chk_green() : border);
            fl_line_style(FL_SOLID, 2);
            fl_line(sx + pad, sy + ss / 2, sx + ss / 2 - 1, sy + ss - pad - 1);
            fl_line(sx + ss / 2 - 1, sy + ss - pad - 1, sx + ss - pad - 1, sy + pad);
        }
    }

    fl_line_style(0);

    if (damage() & FL_DAMAGE_ALL)
        draw_box();

    draw_label(x(), y(), sx - x() - 4, h(), FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
}
