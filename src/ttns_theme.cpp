#include "ttns_theme.h"

#include <string.h>

#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Window.H>

#include "FL/Fl_My_Value_Slider.H"
#include "FL/Fl_Ttns_Cart_Button.H"
#include "FL/Fl_Ttns_Transport_Button.H"
#include "FL/Fl_Ttns_Border_Button.H"
#include "FL/Fl_Ttns_Check_Button.H"
#include "FL/Fl_Ttns_Mic_Button.H"
#include "Fl_ILM216.h"
#include "cfg.h"
#include "flgui.h"

Fl_Color ttns_col_bg(void)    { return fl_rgb_color(0, 0, 0); }
Fl_Color ttns_col_red(void)   { return fl_rgb_color(227, 27, 35); }
/* Classic terminal phosphor green (matches macOS Terminal). */
Fl_Color ttns_col_green(void) { return fl_rgb_color(51, 255, 51); }
Fl_Color ttns_col_fg(void)    { return ttns_col_green(); }
Fl_Color ttns_col_orange(void) { return fl_rgb_color(255, 153, 0); }
Fl_Color ttns_col_meter_red(void) { return fl_rgb_color(255, 48, 48); }
Fl_Color ttns_col_dark(void)  { return fl_rgb_color(24, 24, 24); }
Fl_Color ttns_col_cart_border(void) { return ttns_col_red(); }
Fl_Color ttns_col_yellow(void)    { return fl_rgb_color(255, 220, 0); }

static void ttns_theme_apply_fl_globals(void)
{
    Fl::background(0, 0, 0);
    Fl::background2(0, 0, 0);
    Fl::foreground(51, 255, 51);
}

void ttns_draw_round_frame(int x, int y, int w, int h, Fl_Color fill, Fl_Color border)
{
    int r = TTNS_BTN_CORNER;

    if (w < 8 || h < 8)
        return;

    if (r * 2 + 4 > w)
        r = (w - 4) / 2;
    if (r * 2 + 4 > h)
        r = (h - 4) / 2;
    if (r < 2)
        r = 2;

    fl_color(fill);
    fl_rounded_rectf(x, y, w, h, r);
    fl_color(border);
    fl_line_style(FL_SOLID, 2);
    fl_rounded_rect(x + 1, y + 1, w - 2, h - 2, r);
    fl_line_style(0);
}

void ttns_theme_style_transport_button(Fl_Button *b)
{
    if (!b)
        return;
    b->box(FL_BORDER_BOX);
    b->down_box(FL_BORDER_BOX);
    b->color(ttns_col_bg(), ttns_col_fg());
    b->selection_color(ttns_col_dark());
    b->labelcolor(ttns_col_fg());
}

void ttns_theme_style_slider(Fl_My_Value_Slider *s)
{
    if (!s)
        return;
    s->type(5);
    s->box(FL_NO_BOX);
    s->color(ttns_col_dark());
    s->selection_color(ttns_col_fg());
    s->labelcolor(ttns_col_fg());
}

void ttns_theme_style_choice(Fl_Choice *c)
{
    if (!c)
        return;
    c->box(FL_BORDER_BOX);
    c->down_box(FL_BORDER_BOX);
    c->color(ttns_col_bg(), ttns_col_red());
    c->selection_color(ttns_col_dark());
    c->textcolor(ttns_col_red());
    c->labelcolor(ttns_col_fg());
}

void ttns_theme_style_check(Fl_Check_Button *c)
{
    if (!c)
        return;
    c->box(FL_NO_BOX);
    c->down_box(FL_NO_BOX);
    c->color(ttns_col_bg());
    c->selection_color(ttns_col_red());
    c->labelcolor(ttns_col_fg());
}

void ttns_theme_style_cart_btn(Fl_Button *b)
{
    if (!b)
        return;
    b->box(FL_BORDER_BOX);
    b->color(ttns_col_bg(), ttns_col_cart_border());
    b->selection_color(ttns_col_cart_border());
    b->labelcolor(ttns_col_fg());
}

void ttns_theme_style_label_box(Fl_Box *b)
{
    if (!b)
        return;
    b->color(ttns_col_bg());
    b->labelcolor(ttns_col_fg());
}

void ttns_theme_style_butt_button(Fl_Button *b, int accent_green)
{
    if (!b)
        return;
    b->box(FL_BORDER_BOX);
    b->color(ttns_col_bg());
    b->selection_color(ttns_col_dark());
    b->labelcolor(ttns_col_fg());
}

void ttns_theme_style_window(Fl_Window *w)
{
    if (!w)
        return;
    w->color(ttns_col_bg());
    w->labelcolor(ttns_col_fg());
}

static int ttns_widget_has_image(Fl_Widget *w)
{
    Fl_Box *b = dynamic_cast<Fl_Box*>(w);
    return b && b->image() != NULL;
}

static int ttns_button_accent_green(Fl_Button *b)
{
    const char *tip;

    if (!b)
        return 0;
    tip = b->tooltip();
    if (tip && !strcmp(tip, "connect to server"))
        return 1;
    if (b->label() && !strcmp(b->label(), "&ADD"))
        return 1;
    if (b->label() && !strcmp(b->label(), "&Save"))
        return 1;
    if (b->label() && !strcmp(b->label(), "OK"))
        return 1;
    return 0;
}

static void ttns_style_input(Fl_Input_ *in)
{
    in->color(ttns_col_dark());
    in->textcolor(ttns_col_fg());
    in->selection_color(ttns_col_fg());
    in->cursor_color(ttns_col_fg());
    in->labelcolor(ttns_col_fg());
}

void ttns_theme_apply_widget_tree(Fl_Widget *root)
{
    int i;
    int n;
    Fl_Group *grp;
    Fl_Button *btn;
    Fl_Choice *ch;
    Fl_Check_Button *chk;
    Fl_Round_Button *rb;
    Fl_Input_ *in;
    Fl_Value_Input *vi;
    Fl_Text_Display *td;
    Fl_Tabs *tabs;
    Fl_Box *box;
    Fl_My_Value_Slider *slider;
    Fl_ILM216 *lcd;

    if (!root)
        return;

    if ((lcd = dynamic_cast<Fl_ILM216*>(root)) != NULL)
    {
        lcd->box(FL_NO_BOX);
        lcd->color(ttns_col_bg());
        lcd->selection_color(ttns_col_dark());
        lcd->labelcolor(ttns_col_green());
        return;
    }

    if ((slider = dynamic_cast<Fl_My_Value_Slider*>(root)) != NULL)
    {
        ttns_theme_style_slider(slider);
        return;
    }

    if ((ch = dynamic_cast<Fl_Choice*>(root)) != NULL)
    {
        ttns_theme_style_choice(ch);
        return;
    }

    if ((dynamic_cast<Fl_Ttns_Check_Button*>(root)) != NULL)
        return;

    if ((chk = dynamic_cast<Fl_Check_Button*>(root)) != NULL)
    {
        ttns_theme_style_check(chk);
        return;
    }

    if ((rb = dynamic_cast<Fl_Round_Button*>(root)) != NULL)
    {
        rb->color(ttns_col_bg());
        rb->labelcolor(ttns_col_fg());
        rb->selection_color(ttns_col_fg());
        return;
    }

    if ((btn = dynamic_cast<Fl_Ttns_Cart_Button*>(root)) != NULL)
        return;

    if ((btn = dynamic_cast<Fl_Ttns_Transport_Button*>(root)) != NULL)
        return;

    if ((btn = dynamic_cast<Fl_Ttns_Mic_Button*>(root)) != NULL)
        return;

    if ((btn = dynamic_cast<Fl_Ttns_Border_Button*>(root)) != NULL)
        return;

    if ((btn = dynamic_cast<Fl_Button*>(root)) != NULL)
    {
        const char *tip = btn->tooltip();

        if (tip && (!strcmp(tip, "connect to server") ||
                    !strcmp(tip, "disconnect from server") ||
                    !strcmp(tip, "start/stop recording")))
        {
            ttns_theme_style_transport_button(btn);
            return;
        }

        ttns_theme_style_butt_button(btn, ttns_button_accent_green(btn));
        return;
    }

    if ((in = dynamic_cast<Fl_Input_*>(root)) != NULL)
    {
        ttns_style_input(in);
        return;
    }

    if ((vi = dynamic_cast<Fl_Value_Input*>(root)) != NULL)
    {
        vi->color(ttns_col_dark());
        vi->textcolor(ttns_col_fg());
        vi->selection_color(ttns_col_fg());
        vi->labelcolor(ttns_col_fg());
        return;
    }

    if ((td = dynamic_cast<Fl_Text_Display*>(root)) != NULL)
    {
        td->color(ttns_col_bg());
        td->textcolor(ttns_col_green());
        td->selection_color(ttns_col_fg());
        td->labelcolor(ttns_col_fg());
        return;
    }

    if ((tabs = dynamic_cast<Fl_Tabs*>(root)) != NULL)
    {
        tabs->color(ttns_col_bg());
        tabs->selection_color(ttns_col_red());
        tabs->labelcolor(ttns_col_fg());
        grp = tabs;
    }
    else if ((grp = dynamic_cast<Fl_Group*>(root)) != NULL)
    {
        grp->color(ttns_col_bg());
        grp->labelcolor(ttns_col_fg());
        if (grp->box() == FL_ENGRAVED_FRAME || grp->box() == FL_ENGRAVED_BOX)
        {
            grp->box(FL_BORDER_FRAME);
            grp->color(ttns_col_red());
            grp->labelcolor(ttns_col_fg());
        }
    }
    else if ((box = dynamic_cast<Fl_Box*>(root)) != NULL && !ttns_widget_has_image(root))
    {
        ttns_theme_style_label_box(box);
        return;
    }
    else
    {
        return;
    }

    n = grp->children();
    for (i = 0; i < n; i++)
        ttns_theme_apply_widget_tree(grp->child(i));
}

static void ttns_theme_apply_lcd_cfg(flgui *g)
{
    cfg.main.bg_color = (int)ttns_col_bg();
    cfg.main.txt_color = (int)ttns_col_green();

    if (g->lcd)
        g->lcd->redraw();
}

void ttns_theme_apply(flgui *g)
{
    if (!g)
        return;

    ttns_theme_apply_fl_globals();
    ttns_theme_apply_lcd_cfg(g);

    if (g->window_main)
    {
        ttns_theme_style_window(g->window_main);
        ttns_theme_apply_widget_tree(g->window_main);
    }

    if (g->window_cfg)
    {
        ttns_theme_style_window(g->window_cfg);
        ttns_theme_apply_widget_tree(g->window_cfg);
        if (g->Settings)
        {
            g->Settings->color(ttns_col_bg());
            g->Settings->selection_color(ttns_col_red());
            g->Settings->labelcolor(ttns_col_fg());
        }
    }

    if (g->window_add_srv)
    {
        ttns_theme_style_window(g->window_add_srv);
        ttns_theme_apply_widget_tree(g->window_add_srv);
    }

    if (g->window_add_icy)
    {
        ttns_theme_style_window(g->window_add_icy);
        ttns_theme_apply_widget_tree(g->window_add_icy);
    }
}
