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
    Fl_Color border = active() ? ttns_col_red() : fl_color_average(ttns_col_red(), ttns_col_bg(), 0.35f);
    Fl_Color lab = active() ? ttns_col_fg() : fl_color_average(ttns_col_fg(), ttns_col_bg(), 0.35f);

    ttns_draw_round_frame(x(), y(), w(), h(), ttns_col_bg(), border);
    labelcolor(lab);
    draw_label();
}
