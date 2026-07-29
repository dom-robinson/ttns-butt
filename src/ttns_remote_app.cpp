/*
 * TTNS Remote — standalone co-host client (mic + headphones + connect).
 *
 * Phase 1: UI shell. WebRTC join lands later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include "config.h"

static Fl_Input *room_input = NULL;
static Fl_Box *status_box = NULL;
static char status_text[160] = "Ready — WebRTC transport not linked yet";

static void refresh_status(void)
{
    if (!status_box)
        return;
    status_box->copy_label(status_text);
    status_box->redraw();
}

static void connect_cb(Fl_Widget *, void *)
{
    const char *code;

    if (!room_input)
        return;

    code = room_input->value();
    if (!code || !code[0])
    {
        fl_alert("Enter the room code shown on TTNS Deck.");
        return;
    }

    snprintf(status_text, sizeof(status_text),
             "Cannot join %s yet — signaling/WebRTC arrives in a later phase.",
             code);
    refresh_status();
    fl_alert("%s", status_text);
}

static void disconnect_cb(Fl_Widget *, void *)
{
    snprintf(status_text, sizeof(status_text), "Disconnected");
    refresh_status();
}

int main(int argc, char **argv)
{
    Fl_Window *win;
    Fl_Button *connect_btn;
    Fl_Button *disconnect_btn;
    Fl_Box *title;
    Fl_Box *blurb;
    Fl_Check_Button *mic_mute;
    Fl_Check_Button *ptt_mode;

    Fl::scheme("gtk+");

    win = new Fl_Window(420, 280, "TTNS Remote");

    title = new Fl_Box(12, 12, 396, 28, "TTNS Remote");
    title->labelfont(FL_BOLD);
    title->labelsize(18);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    title->box(FL_NO_BOX);

    blurb = new Fl_Box(12, 48, 396, 44,
                       "Connect mic and headphones, enter the Deck room code, "
                       "then join. You will hear mix-minus and appear as a "
                       "remote channel on the operator mixer.");
    blurb->align(FL_ALIGN_WRAP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    blurb->labelsize(11);
    blurb->box(FL_NO_BOX);

    room_input = new Fl_Input(100, 108, 200, 28, "Room");
    room_input->labelsize(12);
    room_input->textsize(14);

    connect_btn = new Fl_Button(100, 148, 96, 28, "Connect");
    connect_btn->callback(connect_cb);

    disconnect_btn = new Fl_Button(204, 148, 96, 28, "Disconnect");
    disconnect_btn->callback(disconnect_cb);

    mic_mute = new Fl_Check_Button(100, 188, 120, 24, "Mute mic");
    ptt_mode = new Fl_Check_Button(230, 188, 140, 24, "Push-to-talk");
    (void)mic_mute;
    (void)ptt_mode;

    status_box = new Fl_Box(12, 228, 396, 36, "");
    status_box->align(FL_ALIGN_WRAP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_box->labelsize(11);
    status_box->box(FL_NO_BOX);
    refresh_status();

    win->end();
    win->show(argc, argv);
    return Fl::run();
}
