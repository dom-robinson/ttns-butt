#ifndef FL_TTNS_CART_BUTTON_H
#define FL_TTNS_CART_BUTTON_H

#include <FL/Fl_Button.H>

class Fl_Ttns_Cart_Button : public Fl_Button {
    Fl_Color fill_color_;
    Fl_Color text_color_;

public:
    Fl_Ttns_Cart_Button(int x, int y, int w, int h, const char *l = 0);

    void set_fill(Fl_Color c);
    void set_text(Fl_Color c);
    void draw(void);
};

#endif
