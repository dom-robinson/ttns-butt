#ifndef FL_TTNS_CHECK_BUTTON_H
#define FL_TTNS_CHECK_BUTTON_H

#include <FL/Fl_Check_Button.H>

class Fl_Ttns_Check_Button : public Fl_Check_Button {
public:
    Fl_Ttns_Check_Button(int x, int y, int w, int h, const char *l = 0);
    void draw(void);
};

#endif
