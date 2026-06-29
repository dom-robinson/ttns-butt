#ifndef FL_TTNS_MIC_BUTTON_H
#define FL_TTNS_MIC_BUTTON_H

#include <FL/Fl_Button.H>

class Fl_Ttns_Mic_Button : public Fl_Button {
public:
    Fl_Ttns_Mic_Button(int x, int y, int w, int h, const char *l = 0);
    void draw(void);
};

#endif
