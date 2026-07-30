#include "FL/Fl_Ttns_Mic_Button.H"

#include <FL/fl_draw.H>
#include <FL/Enumerations.H>

#include "ttns_theme.h"

/* Compact upright mic: filled capsule + yoke + stand + base. */
static void ttns_draw_mic_glyph(int cx, int cy, int size, int muted)
{
    int cap_w = size * 2 / 5;
    int cap_h = size * 9 / 20;
    int cap_x = cx - cap_w / 2;
    int cap_y = cy - size / 2 + 2;
    int base_y = cy + size / 2 - 5;
    Fl_Color col = muted ? ttns_col_red() : ttns_col_green();

    if (cap_w < 5)
        cap_w = 5;
    if (cap_h < 8)
        cap_h = 8;

    fl_color(col);
#if FL_MINOR_VERSION >= 4
    fl_rounded_rectf(cap_x, cap_y, cap_w, cap_h, cap_w / 2);
#else
    fl_rectf(cap_x, cap_y, cap_w, cap_h);
#endif

    fl_line_style(FL_SOLID, 2);
    fl_color(col);
    fl_line(cx, cap_y + cap_h, cx, base_y - 3);
    fl_line(cx - cap_w / 2 - 2, base_y, cx + cap_w / 2 + 2, base_y);
    fl_arc(cx - cap_w / 2 - 4, cap_y + cap_h - 2, cap_w + 8, cap_h,
           0.0, 180.0);

    if (muted)
    {
        fl_color(ttns_col_red());
        fl_line_style(FL_SOLID, 3);
        fl_line(cx - size / 2 + 2, cy - size / 2 + 2,
                cx + size / 2 - 2, cy + size / 2 - 2);
        fl_line(cx + size / 2 - 2, cy - size / 2 + 2,
                cx - size / 2 + 2, cy + size / 2 - 2);
    }

    fl_line_style(0);
}

Fl_Ttns_Mic_Button::Fl_Ttns_Mic_Button(int x, int y, int w, int h, const char *l)
    : Fl_Button(x, y, w, h, l)
{
    type(FL_TOGGLE_BUTTON);
    box(FL_NO_BOX);
    color(ttns_col_bg());
    labelcolor(ttns_col_fg());
    selection_color(ttns_col_bg());
    copy_label("");
}

void Fl_Ttns_Mic_Button::draw(void)
{
    int pad = 5;
    int icon_h = h() - pad * 2;
    Fl_Color border = ttns_col_red();

    if (!active())
        border = fl_color_average(border, ttns_col_bg(), 0.45f);

    if (icon_h > w() - pad * 2)
        icon_h = w() - pad * 2;

    ttns_draw_round_frame(x(), y(), w(), h(), ttns_col_bg(), border);
    ttns_draw_mic_glyph(x() + w() / 2, y() + h() / 2, icon_h, value());
}
