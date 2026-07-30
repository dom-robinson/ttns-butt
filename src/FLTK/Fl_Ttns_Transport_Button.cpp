#include "FL/Fl_Ttns_Transport_Button.H"

#include <FL/fl_draw.H>

#include "ttns_theme.h"

Fl_Ttns_Transport_Button::Fl_Ttns_Transport_Button(int x, int y, int w, int h, const char *l)
    : Fl_Button(x, y, w, h, l)
{
    box(FL_NO_BOX);
    color(ttns_col_bg());
    labelcolor(ttns_col_fg());
}

void Fl_Ttns_Transport_Button::draw(void)
{
    /*
     * Always use a clear red frame. When deactivated (Confirm unticked),
     * keep the icon bright — a dimmed @> on black looked like a stray
     * triangle under the VU while the empty frame sat in the transport row.
     */
    Fl_Color border = ttns_col_red();
    Fl_Color fill = active() ? color() : fl_color_average(color(), ttns_col_bg(), 0.85f);
    Fl_Color lab = ttns_col_fg();

    ttns_draw_round_frame(x(), y(), w(), h(), fill, border);
    labelcolor(lab);
    draw_label();
}
