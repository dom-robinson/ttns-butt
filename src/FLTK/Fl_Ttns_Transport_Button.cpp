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
    ttns_draw_round_frame(x(), y(), w(), h(), ttns_col_bg(), ttns_col_red());
    draw_label();
}
