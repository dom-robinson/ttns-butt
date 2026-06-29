#include "FL/Fl_Ttns_Check_Button.H"

#include <FL/fl_draw.H>

#include "ttns_theme.h"

Fl_Ttns_Check_Button::Fl_Ttns_Check_Button(int x, int y, int w, int h, const char *l)
    : Fl_Check_Button(x, y, w, h, l)
{
    box(FL_NO_BOX);
    down_box(FL_NO_BOX);
    color(ttns_col_bg());
    selection_color(ttns_col_red());
    labelcolor(ttns_col_fg());
}

void Fl_Ttns_Check_Button::draw(void)
{
    int ss = 14;
    int sx = x() + w() - ss - 2;
    int sy = y() + (h() - ss) / 2;
    Fl_Color border = ttns_col_red();
    Fl_Color fill = value() ? ttns_col_red() : ttns_col_bg();

    if (!active())
    {
        border = fl_color_average(border, ttns_col_bg(), 0.5f);
        fill = fl_color_average(fill, ttns_col_bg(), 0.5f);
    }

    fl_color(fill);
    fl_rectf(sx, sy, ss, ss);
    fl_color(border);
    fl_line_style(FL_SOLID, 2);
    fl_rect(sx, sy, ss, ss);
    fl_line_style(0);

    if (damage() & FL_DAMAGE_ALL)
        draw_box();

    draw_label(x(), y(), sx - x() - 4, h(), FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
}
