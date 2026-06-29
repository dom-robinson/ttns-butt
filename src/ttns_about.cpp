#include "ttns_about.h"

#include <stdio.h>
#include <limits.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include "config.h"
#include "ttns_paths.h"

void ttns_set_window_icon(Fl_Window *win)
{
    char path[PATH_MAX];
    Fl_RGB_Image *rgb = NULL;

    if (!win)
        return;

    if (ttns_path_asset_file("ttns-logo.png", path, sizeof(path)) != 0)
        return;

    {
        Fl_PNG_Image *png = new Fl_PNG_Image(path);
        if (png && png->w() > 0 && png->h() > 0)
        {
            rgb = (Fl_RGB_Image*)png->copy(32, 32);
            delete png;
        }
        else
        {
            delete png;
        }
    }

    if (rgb)
    {
        win->icon(rgb);
    }
}

void ttns_show_about(void)
{
    char body[1024];

    snprintf(body, sizeof(body),
             "TTNS Deck %s\n"
             "\n"
             "Live broadcast mixer for The Thursday Night Show.\n"
             "Streams to decks.thethursdaynightshow.com via Icecast.\n"
             "\n"
             "Features: dual mic/line mix, ducking, 8 cart slots,\n"
             "zone/slot presets, post-mix recording.\n"
             "\n"
             "Based on butt (Broadcast Using This Tool) %s\n"
             "Original author: Daniel Nöthen — GPL-2.0\n"
             "\n"
             "TTNS fork: github.com/dom-robinson/ttns-butt\n"
             "Support: https://thethursdaynightshow.com",
             PACKAGE_VERSION, VERSION);

    fl_message("%s", body);
}
