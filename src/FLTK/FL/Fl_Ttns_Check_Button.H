#ifndef FL_TTNS_CHECK_BUTTON_H
#define FL_TTNS_CHECK_BUTTON_H

#include <FL/Fl_Check_Button.H>

/* Affirm: green check when on (Confirm, Mic mon, Accept, …).
 * Negate: red X when on (mute / deactivate). */
enum {
    TTNS_CHECK_AFFIRM = 0,
    TTNS_CHECK_NEGATE = 1
};

class Fl_Ttns_Check_Button : public Fl_Check_Button {
    int indicator_;
public:
    Fl_Ttns_Check_Button(int x, int y, int w, int h, const char *l = 0);
    void indicator(int mode);
    int indicator(void) const { return indicator_; }
    void draw(void);
};

#endif
