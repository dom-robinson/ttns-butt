#include "FL/Fl_Ttns_Cart_Button.H"

#include <FL/fl_draw.H>

#include "ttns_theme.h"

Fl_Ttns_Cart_Button::Fl_Ttns_Cart_Button(int x, int y, int w, int h, const char *l)
    : Fl_Button(x, y, w, h, l)
{
    box(FL_NO_BOX);
    fill_color_ = ttns_col_bg();
    text_color_ = ttns_col_fg();
    color(fill_color_);
    labelcolor(text_color_);
}

void Fl_Ttns_Cart_Button::set_fill(Fl_Color c)
{
    if (fill_color_ == c)
        return;
    fill_color_ = c;
    color(c);
    redraw();
}

void Fl_Ttns_Cart_Button::set_text(Fl_Color c)
{
    if (text_color_ == c)
        return;
    text_color_ = c;
    labelcolor(c);
    redraw();
}

void Fl_Ttns_Cart_Button::draw(void)
{
    ttns_draw_round_frame(x(), y(), w(), h(), fill_color_, ttns_col_cart_border());

    fl_color(text_color_);
    draw_label();
}
