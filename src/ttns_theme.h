#ifndef TTNS_THEME_H
#define TTNS_THEME_H

#include <FL/Fl.H>

class Fl_Box;
class Fl_Button;
class Fl_Check_Button;
class Fl_Choice;
class Fl_My_Value_Slider;
class Fl_Widget;
class Fl_Window;
class flgui;

Fl_Color ttns_col_bg(void);
Fl_Color ttns_col_red(void);
Fl_Color ttns_col_green(void);
Fl_Color ttns_col_fg(void);
Fl_Color ttns_col_orange(void);
Fl_Color ttns_col_meter_red(void);
Fl_Color ttns_col_dark(void);
Fl_Color ttns_col_cart_border(void);
Fl_Color ttns_col_yellow(void);

#define TTNS_BTN_CORNER 5

void ttns_draw_round_frame(int x, int y, int w, int h, Fl_Color fill, Fl_Color border);

void ttns_theme_style_transport_button(Fl_Button *b);
void ttns_theme_style_slider(Fl_My_Value_Slider *s);
void ttns_theme_style_choice(Fl_Choice *c);
void ttns_theme_style_check(Fl_Check_Button *c);
void ttns_theme_style_cart_btn(Fl_Button *b);
void ttns_theme_style_label_box(Fl_Box *b);
void ttns_theme_style_butt_button(Fl_Button *b, int accent_green);
void ttns_theme_style_window(Fl_Window *w);
void ttns_theme_apply_widget_tree(Fl_Widget *root);
void ttns_theme_apply(flgui *g);

#endif
