#ifndef FL_TTNS_FADER_H
#define FL_TTNS_FADER_H

#include "FL/Fl_My_Value_Slider.H"

class Fl_Ttns_Fader : public Fl_My_Value_Slider {
    float level_;
    float peak_;
    double peak_until_sec_;
    int meter_enabled_;

public:
    static const int TRACK_H = 14;

    Fl_Ttns_Fader(int x, int y, int w, int h, const char *l = 0, int meter_enabled = 1);

    void set_peak_sample(int sample16);
    int handle(int e);
    void draw(void);
};

#endif
