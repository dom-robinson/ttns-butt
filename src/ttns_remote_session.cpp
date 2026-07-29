#include "ttns_remote_session.h"

#include <stdio.h>
#include <string.h>

#include "cfg.h"
#include "ttns_remote.h"

static int host_running = 0;
static int client_connected = 0;
static char status_text[128] = "idle";

int ttns_remote_session_init(void)
{
    host_running = 0;
    client_connected = 0;
    snprintf(status_text, sizeof(status_text), "idle (transport not linked yet)");
    return 0;
}

void ttns_remote_session_shutdown(void)
{
    ttns_remote_session_host_stop();
    ttns_remote_session_client_leave();
}

int ttns_remote_session_host_start(void)
{
    if (cfg.ttns.remote_room[0] == '\0')
    {
        char code[TTNS_REMOTE_ROOM_LEN];
        ttns_remote_generate_room_code(code, sizeof(code));
        ttns_remote_set_room_code(code);
    }

    host_running = 1;
    cfg.ttns.remote_accept = 1;
    snprintf(status_text, sizeof(status_text),
             "host ready — code %s (WebRTC pending)", ttns_remote_room_code());
    return 0;
}

void ttns_remote_session_host_stop(void)
{
    int i;

    host_running = 0;
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (!ttns_remote_test_tone(i))
            ttns_remote_clear_slot(i);
    }
    snprintf(status_text, sizeof(status_text), "host stopped");
}

int ttns_remote_session_host_running(void)
{
    return host_running;
}

int ttns_remote_session_client_join(const char *room_code)
{
    if (!room_code || !room_code[0])
        return 1;

    client_connected = 0;
    snprintf(status_text, sizeof(status_text),
             "join %s failed — WebRTC/signaling not implemented yet", room_code);
    return 1;
}

void ttns_remote_session_client_leave(void)
{
    client_connected = 0;
}

int ttns_remote_session_client_connected(void)
{
    return client_connected;
}

const char *ttns_remote_session_status_text(void)
{
    return status_text;
}
