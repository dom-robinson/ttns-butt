#include "ttns_remote_session.h"

#include <opus/opus.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "cfg.h"
#include "ttns_remote.h"
#include "ttns_remote_net.h"
#include "ttns_remote_proto.h"
#include "ttns_remote_wan.h"

typedef struct {
    int fd;
    int slot;
    int active;
    int via_wan;
    pthread_t rx_thread;
    pthread_t tx_thread;
    OpusDecoder *dec;
    OpusEncoder *enc;
    uint16_t tx_seq;
    char name[TTNS_REMOTE_NAME_MAX];
} ttns_peer_t;

static ttns_peer_t peers[TTNS_REMOTE_SLOTS];
static int listen_fd = -1;
static int listen_port = 0;
static pthread_t accept_thread;
static int accept_thread_alive = 0;
static pthread_t wan_rx_thread;
static int wan_rx_alive = 0;
static volatile int host_running = 0;
static volatile int client_connected = 0;
static char status_text[160] = "idle";
static pthread_mutex_t session_mtx = PTHREAD_MUTEX_INITIALIZER;

static void ttns_peer_clear(ttns_peer_t *p);
static int ttns_peer_send_audio(ttns_peer_t *p, const unsigned char *opus, int nbytes);

static void ttns_peer_clear(ttns_peer_t *p)
{
    if (!p)
        return;
    p->active = 0;
    if (p->fd >= 0)
    {
        ttns_rnet_send_packet(p->fd, TTNS_PKT_BYE, (uint8_t)p->slot, 0, NULL, 0);
        ttns_rnet_close(&p->fd);
    }
    if (p->dec)
    {
        opus_decoder_destroy(p->dec);
        p->dec = NULL;
    }
    if (p->enc)
    {
        opus_encoder_destroy(p->enc);
        p->enc = NULL;
    }
    if (p->slot >= 0 && p->slot < TTNS_REMOTE_SLOTS)
        ttns_remote_clear_slot(p->slot);
    p->slot = -1;
    p->name[0] = '\0';
    p->tx_seq = 0;
    p->via_wan = 0;
}

static void *ttns_peer_rx(void *arg)
{
    ttns_peer_t *p = (ttns_peer_t *)arg;
    ttns_remote_hdr_t hdr;
    unsigned char payload[TTNS_REMOTE_MAX_PACKET];
    short pcm[TTNS_REMOTE_FRAME_SAMPLES];
    int n;

    while (p->active && host_running)
    {
        n = ttns_rnet_recv_packet(p->fd, &hdr, payload, sizeof(payload), 200);
        if (n < 0)
            break;
        if (n == 0)
            continue;
        if (hdr.type == TTNS_PKT_BYE)
            break;
        if (hdr.type == TTNS_PKT_AUDIO && p->dec)
        {
            int frames = opus_decode(p->dec, payload, n, pcm,
                                     TTNS_REMOTE_FRAME_SAMPLES, 0);
            if (frames > 0)
                ttns_remote_push_uplink(p->slot, pcm, frames, 1);
        }
    }

    pthread_mutex_lock(&session_mtx);
    if (p->active)
    {
        p->active = 0;
        ttns_rnet_close(&p->fd);
        if (p->slot >= 0)
            ttns_remote_clear_slot(p->slot);
        snprintf(status_text, sizeof(status_text),
                 "peer left slot %d — room %s", p->slot + 1, ttns_remote_room_code());
    }
    pthread_mutex_unlock(&session_mtx);
    return NULL;
}

static void *ttns_peer_tx(void *arg)
{
    ttns_peer_t *p = (ttns_peer_t *)arg;
    short pcm[TTNS_REMOTE_FRAME_SAMPLES * 2];
    unsigned char opus_buf[TTNS_REMOTE_MAX_PACKET];
    short mono[TTNS_REMOTE_FRAME_SAMPLES];
    int idle_waits = 0;
    struct timespec next_due;

    clock_gettime(CLOCK_MONOTONIC, &next_due);

    while (p->active && host_running)
    {
        int i;
        int nbytes;
        struct timespec now;
        struct timespec ts;

        /* Pace to real-time 20 ms frames so we never drain the mix ring dry and
         * inject silence keepalive gaps into an otherwise continuous bed. */
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec < next_due.tv_sec
            || (now.tv_sec == next_due.tv_sec && now.tv_nsec < next_due.tv_nsec))
        {
            ts.tv_sec = next_due.tv_sec - now.tv_sec;
            ts.tv_nsec = next_due.tv_nsec - now.tv_nsec;
            if (ts.tv_nsec < 0)
            {
                ts.tv_sec--;
                ts.tv_nsec += 1000000000L;
            }
            nanosleep(&ts, NULL);
        }
        next_due.tv_nsec += (long)TTNS_REMOTE_FRAME_MS * 1000000L;
        if (next_due.tv_nsec >= 1000000000L)
        {
            next_due.tv_sec++;
            next_due.tv_nsec -= 1000000000L;
        }
        /* If we fell more than 100 ms behind, resync rather than catch-up burst. */
        clock_gettime(CLOCK_MONOTONIC, &now);
        {
            long lag_ns = (long)(now.tv_sec - next_due.tv_sec) * 1000000000L
                + (now.tv_nsec - next_due.tv_nsec);
            if (lag_ns > 100000000L)
                next_due = now;
        }

        /* Wait for a full Opus frame — never consume partials. */
        if (ttns_remote_mix_minus_avail(p->slot) < TTNS_REMOTE_FRAME_SAMPLES)
        {
            idle_waits++;
            /* Only after ~500 ms with no mix (startup / pause) send silence. */
            if (idle_waits < 25)
                continue;
            idle_waits = 0;
            memset(mono, 0, sizeof(mono));
            nbytes = opus_encode(p->enc, mono, TTNS_REMOTE_FRAME_SAMPLES, opus_buf,
                                 sizeof(opus_buf));
            if (nbytes > 0)
            {
                if (ttns_peer_send_audio(p, opus_buf, nbytes) != 0)
                    break;
            }
            continue;
        }

        idle_waits = 0;
        if (ttns_remote_read_mix_minus(p->slot, pcm, TTNS_REMOTE_FRAME_SAMPLES)
            < TTNS_REMOTE_FRAME_SAMPLES)
            continue;

        for (i = 0; i < TTNS_REMOTE_FRAME_SAMPLES; i++)
            mono[i] = (short)(((int)pcm[i * 2] + (int)pcm[i * 2 + 1]) / 2);

        nbytes = opus_encode(p->enc, mono, TTNS_REMOTE_FRAME_SAMPLES, opus_buf,
                             sizeof(opus_buf));
        if (nbytes > 0)
        {
            if (ttns_peer_send_audio(p, opus_buf, nbytes) != 0)
                break;
        }
    }
    return NULL;
}

static int ttns_peer_send_audio(ttns_peer_t *p, const unsigned char *opus, int nbytes)
{
    if (p->via_wan)
    {
        unsigned char pkt[sizeof(ttns_remote_hdr_t) + TTNS_REMOTE_MAX_PACKET];
        int total = ttns_rnet_build_packet(pkt, sizeof(pkt), TTNS_PKT_AUDIO,
                                           (uint8_t)p->slot, p->tx_seq++, opus,
                                           (uint16_t)nbytes);
        if (total < 0)
            return -1;
        return ttns_wan_host_send(pkt, (size_t)total);
    }
    return ttns_rnet_send_packet(p->fd, TTNS_PKT_AUDIO, (uint8_t)p->slot,
                                 p->tx_seq++, opus, (uint16_t)nbytes);
}

static int ttns_alloc_slot_locked(void)
{
    int i;
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (!peers[i].active && peers[i].fd < 0)
            return i;
    }
    /* Also allow slots whose peer struct is free even if remote state busy from test */
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (!peers[i].active)
            return i;
    }
    return -1;
}

static int ttns_start_wan_peer_locked(int slot, const char *name)
{
    int err;
    ttns_peer_t *p;

    if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
        return -1;
    p = &peers[slot];
    if (p->active)
        return -1;

    ttns_peer_clear(p);
    p->fd = -1;
    p->slot = slot;
    p->via_wan = 1;
    snprintf(p->name, sizeof(p->name), "%s", name && name[0] ? name : "Remote");

    p->dec = opus_decoder_create(TTNS_REMOTE_SAMPLERATE, 1, &err);
    if (!p->dec)
        return -1;
    p->enc = opus_encoder_create(TTNS_REMOTE_SAMPLERATE, 1, OPUS_APPLICATION_AUDIO, &err);
    if (!p->enc)
    {
        opus_decoder_destroy(p->dec);
        p->dec = NULL;
        return -1;
    }
    opus_encoder_ctl(p->enc, OPUS_SET_BITRATE(TTNS_REMOTE_OPUS_BR_DOWN));
    opus_encoder_ctl(p->enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(p->enc, OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(p->enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(p->enc, OPUS_SET_INBAND_FEC(1));

    ttns_remote_set_name(slot, p->name);
    ttns_remote_set_state(slot, TTNS_REMOTE_CONNECTED);
    p->active = 1;

    if (pthread_create(&p->tx_thread, NULL, ttns_peer_tx, p) != 0)
    {
        p->active = 0;
        ttns_peer_clear(p);
        return -1;
    }
    pthread_detach(p->tx_thread);

    snprintf(status_text, sizeof(status_text),
             "WAN connected %s on R%d — room %s", p->name, slot + 1,
             ttns_remote_room_code());
    return slot;
}

static void *ttns_wan_rx_thread(void *arg)
{
    unsigned char buf[sizeof(ttns_remote_hdr_t) + TTNS_REMOTE_MAX_PACKET];
    (void)arg;

    while (host_running && ttns_wan_host_running())
    {
        int n = ttns_wan_host_recv(buf, sizeof(buf), 200);
        ttns_remote_hdr_t *h;
        int slot;
        int plen;
        short pcm[TTNS_REMOTE_FRAME_SAMPLES];

        if (n < (int)sizeof(ttns_remote_hdr_t))
            continue;
        h = (ttns_remote_hdr_t *)buf;
        if (h->magic != 0x53544E54u)
            continue;
        slot = h->slot;
        plen = (int)ntohs(h->len);
        if (slot < 0 || slot >= TTNS_REMOTE_SLOTS)
            continue;
        if (n < (int)sizeof(*h) + plen)
            continue;
        if (h->type == TTNS_PKT_AUDIO && peers[slot].active && peers[slot].via_wan
            && peers[slot].dec)
        {
            int frames = opus_decode(peers[slot].dec, buf + sizeof(*h), plen, pcm,
                                     TTNS_REMOTE_FRAME_SAMPLES, 0);
            if (frames > 0)
                ttns_remote_push_uplink(slot, pcm, frames, 1);
        }
        else if (h->type == TTNS_PKT_BYE && peers[slot].active && peers[slot].via_wan)
        {
            pthread_mutex_lock(&session_mtx);
            peers[slot].active = 0;
            ttns_remote_clear_slot(slot);
            ttns_peer_clear(&peers[slot]);
            pthread_mutex_unlock(&session_mtx);
        }
    }
    wan_rx_alive = 0;
    return NULL;
}

static int ttns_start_peer_locked(int fd, const char *name)
{
    int slot;
    int err;
    ttns_peer_t *p;
    unsigned char ack;

    slot = ttns_alloc_slot_locked();
    if (slot < 0)
        return -1;

    p = &peers[slot];
    ttns_peer_clear(p);
    p->fd = fd;
    p->slot = slot;
    snprintf(p->name, sizeof(p->name), "%s", name && name[0] ? name : "Remote");

    p->dec = opus_decoder_create(TTNS_REMOTE_SAMPLERATE, 1, &err);
    if (!p->dec)
        return -1;
    /* Downlink is mix-minus (music + carts + mics) — AUDIO mode, not VOIP. */
    p->enc = opus_encoder_create(TTNS_REMOTE_SAMPLERATE, 1, OPUS_APPLICATION_AUDIO, &err);
    if (!p->enc)
    {
        opus_decoder_destroy(p->dec);
        p->dec = NULL;
        return -1;
    }
    opus_encoder_ctl(p->enc, OPUS_SET_BITRATE(TTNS_REMOTE_OPUS_BR_DOWN));
    opus_encoder_ctl(p->enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(p->enc, OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(p->enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(p->enc, OPUS_SET_INBAND_FEC(1));

    ack = (unsigned char)slot;
    if (ttns_rnet_send_packet(fd, TTNS_PKT_HELLO_ACK, (uint8_t)slot, 0, &ack, 1) != 0)
    {
        ttns_peer_clear(p);
        return -1;
    }

    ttns_remote_set_name(slot, p->name);
    ttns_remote_set_state(slot, TTNS_REMOTE_CONNECTED);
    p->active = 1;

    if (pthread_create(&p->rx_thread, NULL, ttns_peer_rx, p) != 0)
    {
        ttns_peer_clear(p);
        return -1;
    }
    pthread_detach(p->rx_thread);
    if (pthread_create(&p->tx_thread, NULL, ttns_peer_tx, p) != 0)
    {
        p->active = 0;
        ttns_rnet_close(&p->fd);
        return -1;
    }
    pthread_detach(p->tx_thread);

    snprintf(status_text, sizeof(status_text),
             "connected %s on R%d — room %s", p->name, slot + 1, ttns_remote_room_code());
    return slot;
}

static void *ttns_accept_thread(void *arg)
{
    (void)arg;
    while (host_running)
    {
        char ip[64];
        int fd;
        ttns_remote_hdr_t hdr;
        unsigned char payload[96];
        char name[TTNS_REMOTE_NAME_MAX];
        int n;
        int wan_slot = -1;

        /* WAN peer joins announced by signaling */
        if (ttns_wan_host_accept_peer(&wan_slot, name, sizeof(name)))
        {
            pthread_mutex_lock(&session_mtx);
            if (wan_slot >= 0 && wan_slot < TTNS_REMOTE_SLOTS
                && ttns_start_wan_peer_locked(wan_slot, name) < 0)
            {
                /* slot collision — ignore */
            }
            pthread_mutex_unlock(&session_mtx);
        }

        fd = ttns_rnet_accept(listen_fd, ip, sizeof(ip), 200);
        if (fd < 0)
            continue;

        n = ttns_rnet_recv_packet(fd, &hdr, payload, sizeof(payload), 3000);
        if (n < 1 || hdr.type != TTNS_PKT_HELLO)
        {
            ttns_rnet_close(&fd);
            continue;
        }

        payload[n < (int)sizeof(payload) ? n : (int)sizeof(payload) - 1] = 0;
        {
            char got[TTNS_REMOTE_ROOM_LEN];
            char want[TTNS_REMOTE_ROOM_LEN];

            ttns_remote_normalize_room(got, sizeof(got), (char *)payload);
            ttns_remote_normalize_room(want, sizeof(want), ttns_remote_room_code());
            if (got[0] == '\0' || strcmp(got, want) != 0)
            {
                ttns_rnet_close(&fd);
                continue;
            }
        }

        name[0] = '\0';
        {
            int roomlen = (int)strlen((char *)payload);
            if (n > roomlen + 1)
            {
                int namelen = n - roomlen - 1;
                if (namelen >= TTNS_REMOTE_NAME_MAX)
                    namelen = TTNS_REMOTE_NAME_MAX - 1;
                memcpy(name, payload + roomlen + 1, (size_t)namelen);
                name[namelen] = '\0';
            }
        }

        pthread_mutex_lock(&session_mtx);
        if (ttns_start_peer_locked(fd, name) < 0)
            ttns_rnet_close(&fd);
        pthread_mutex_unlock(&session_mtx);
    }
    return NULL;
}

int ttns_remote_session_init(void)
{
    int i;
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        peers[i].fd = -1;
        peers[i].slot = -1;
        peers[i].active = 0;
        peers[i].dec = NULL;
        peers[i].enc = NULL;
    }
    host_running = 0;
    client_connected = 0;
    snprintf(status_text, sizeof(status_text), "idle");
    return 0;
}

void ttns_remote_session_shutdown(void)
{
    ttns_remote_session_host_stop();
    ttns_remote_session_client_leave();
}

int ttns_remote_session_host_start(void)
{
    int port;
    int fd = -1;

    if (host_running)
        return 0;

    if (cfg.ttns.remote_room[0] == '\0')
    {
        char code[TTNS_REMOTE_ROOM_LEN];
        ttns_remote_generate_room_code(code, sizeof(code));
        ttns_remote_set_room_code(code);
    }

    for (port = TTNS_REMOTE_TCP_PORT_BASE; port < TTNS_REMOTE_TCP_PORT_BASE + 20; port++)
    {
        fd = ttns_rnet_listen(port);
        if (fd >= 0)
            break;
    }
    if (fd < 0)
    {
        snprintf(status_text, sizeof(status_text), "host listen failed");
        return 1;
    }

    listen_fd = fd;
    listen_port = port;
    host_running = 1;
    cfg.ttns.remote_accept = 1;

    if (ttns_rnet_discovery_reply_start(ttns_remote_room_code(), listen_port) != 0)
    {
        snprintf(status_text, sizeof(status_text),
                 "listening :%d but LAN discovery bind failed — room %s",
                 listen_port, ttns_remote_room_code());
    }
    else
    {
        snprintf(status_text, sizeof(status_text),
                 "accepting on LAN — room %s (tcp %d)", ttns_remote_room_code(), listen_port);
    }

    if (pthread_create(&accept_thread, NULL, ttns_accept_thread, NULL) != 0)
    {
        host_running = 0;
        ttns_rnet_discovery_reply_stop();
        ttns_rnet_close(&listen_fd);
        snprintf(status_text, sizeof(status_text), "accept thread failed");
        return 1;
    }
    accept_thread_alive = 1;

    /* Also publish room on WRX so remotes can join from the internet. */
    if (ttns_wan_available()
        && ttns_wan_host_start(ttns_remote_room_code(), "Deck") == 0)
    {
        wan_rx_alive = 1;
        if (pthread_create(&wan_rx_thread, NULL, ttns_wan_rx_thread, NULL) != 0)
            wan_rx_alive = 0;
        else
            pthread_detach(wan_rx_thread);
        snprintf(status_text, sizeof(status_text),
                 "accepting LAN+WAN — room %s", ttns_remote_room_code());
    }
    else if (ttns_wan_available())
    {
        snprintf(status_text, sizeof(status_text),
                 "accepting LAN — room %s (WAN offline: %s)",
                 ttns_remote_room_code(), ttns_wan_last_error());
    }
    return 0;
}

void ttns_remote_session_host_stop(void)
{
    int i;

    if (!host_running && listen_fd < 0)
        return;

    host_running = 0;
    ttns_wan_host_stop();
    ttns_rnet_discovery_reply_stop();
    ttns_rnet_close(&listen_fd);
    if (accept_thread_alive)
    {
        pthread_join(accept_thread, NULL);
        accept_thread_alive = 0;
    }
    wan_rx_alive = 0;

    pthread_mutex_lock(&session_mtx);
    for (i = 0; i < TTNS_REMOTE_SLOTS; i++)
    {
        if (peers[i].active || peers[i].fd >= 0)
            ttns_peer_clear(&peers[i]);
    }
    pthread_mutex_unlock(&session_mtx);

    snprintf(status_text, sizeof(status_text), "host stopped");
}

int ttns_remote_session_host_running(void)
{
    return host_running;
}

void ttns_remote_session_host_refresh_discovery(void)
{
    if (!host_running || listen_port <= 0)
        return;
    if (ttns_rnet_discovery_reply_start(ttns_remote_room_code(), listen_port) != 0)
    {
        snprintf(status_text, sizeof(status_text),
                 "listening :%d but LAN discovery bind failed — room %s",
                 listen_port, ttns_remote_room_code());
    }
    else
    {
        snprintf(status_text, sizeof(status_text),
                 "accepting LAN+WAN — room %s", ttns_remote_room_code());
    }
    /* Re-publish on WRX with the new room code. */
    ttns_wan_host_stop();
    if (ttns_wan_available()
        && ttns_wan_host_start(ttns_remote_room_code(), "Deck") == 0)
    {
        if (!wan_rx_alive)
        {
            wan_rx_alive = 1;
            if (pthread_create(&wan_rx_thread, NULL, ttns_wan_rx_thread, NULL) != 0)
                wan_rx_alive = 0;
            else
                pthread_detach(wan_rx_thread);
        }
    }
}

/* Client path lives in the remote app; stubs keep the Deck link happy. */
int ttns_remote_session_client_join(const char *room_code)
{
    (void)room_code;
    client_connected = 0;
    snprintf(status_text, sizeof(status_text),
             "use TTNS Remote app to join a room");
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
