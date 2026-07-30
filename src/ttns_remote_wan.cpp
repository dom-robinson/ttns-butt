/*
 * TTNS WAN transport — WebSocket relay via WRX (ttns-signal).
 * Uses libcurl CONNECT_ONLY WebSocket mode (Homebrew curl on macOS).
 */

#include "ttns_remote_wan.h"

#include "ttns_remote_proto.h"
#include "ttns_remote_net.h"

#include <curl/curl.h>
#include <curl/websockets.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#define WAN_Q_MAX 64
#define WAN_PKT_MAX TTNS_REMOTE_MAX_PACKET
#define WAN_URL_MAX 256

typedef struct {
    unsigned char data[WAN_PKT_MAX];
    int len;
} wan_pkt_t;

typedef struct {
    wan_pkt_t pkts[WAN_Q_MAX];
    int head;
    int tail;
    int count;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} wan_queue_t;

typedef struct {
    int slot;
    char name[TTNS_REMOTE_NAME_MAX];
} wan_peer_evt_t;

typedef struct {
    wan_peer_evt_t ev[WAN_Q_MAX];
    int head;
    int tail;
    int count;
    pthread_mutex_t mtx;
} wan_peer_q_t;

static void wan_q_init(wan_queue_t *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cv, NULL);
}

static void wan_q_free(wan_queue_t *q)
{
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cv);
}

static int wan_q_push(wan_queue_t *q, const void *data, int len)
{
    if (!data || len < 1 || len > WAN_PKT_MAX)
        return -1;
    pthread_mutex_lock(&q->mtx);
    if (q->count >= WAN_Q_MAX)
    {
        /* Drop oldest */
        q->head = (q->head + 1) % WAN_Q_MAX;
        q->count--;
    }
    memcpy(q->pkts[q->tail].data, data, (size_t)len);
    q->pkts[q->tail].len = len;
    q->tail = (q->tail + 1) % WAN_Q_MAX;
    q->count++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
    return 0;
}

static int wan_q_pop(wan_queue_t *q, void *buf, size_t buflen, int timeout_ms)
{
    int len = 0;
    struct timespec ts;

    pthread_mutex_lock(&q->mtx);
    if (q->count < 1 && timeout_ms > 0)
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += (long)timeout_ms * 1000000L;
        if (ts.tv_nsec >= 1000000000L)
        {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;
        }
        while (q->count < 1)
        {
            if (pthread_cond_timedwait(&q->cv, &q->mtx, &ts) != 0)
                break;
        }
    }
    if (q->count < 1)
    {
        pthread_mutex_unlock(&q->mtx);
        return 0;
    }
    len = q->pkts[q->head].len;
    if ((size_t)len > buflen)
        len = (int)buflen;
    memcpy(buf, q->pkts[q->head].data, (size_t)len);
    q->head = (q->head + 1) % WAN_Q_MAX;
    q->count--;
    pthread_mutex_unlock(&q->mtx);
    return len;
}

static void wan_peer_q_init(wan_peer_q_t *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->mtx, NULL);
}

static void wan_peer_q_free(wan_peer_q_t *q)
{
    pthread_mutex_destroy(&q->mtx);
}

static void wan_peer_q_push(wan_peer_q_t *q, int slot, const char *name)
{
    pthread_mutex_lock(&q->mtx);
    if (q->count >= WAN_Q_MAX)
    {
        q->head = (q->head + 1) % WAN_Q_MAX;
        q->count--;
    }
    q->ev[q->tail].slot = slot;
    snprintf(q->ev[q->tail].name, sizeof(q->ev[q->tail].name), "%s",
             name && name[0] ? name : "Remote");
    q->tail = (q->tail + 1) % WAN_Q_MAX;
    q->count++;
    pthread_mutex_unlock(&q->mtx);
}

static int wan_peer_q_pop(wan_peer_q_t *q, int *slot, char *name, size_t namelen)
{
    pthread_mutex_lock(&q->mtx);
    if (q->count < 1)
    {
        pthread_mutex_unlock(&q->mtx);
        return 0;
    }
    if (slot)
        *slot = q->ev[q->head].slot;
    if (name && namelen)
        snprintf(name, namelen, "%s", q->ev[q->head].name);
    q->head = (q->head + 1) % WAN_Q_MAX;
    q->count--;
    pthread_mutex_unlock(&q->mtx);
    return 1;
}

typedef struct {
    int active;
    int is_host;
    int slot;
    int joined;
    char room[16];
    char name[32];
    char url[WAN_URL_MAX];
    char err[160];
    CURL *curl;
    pthread_t thr;
    wan_queue_t rx_q;
    wan_queue_t tx_q;
    wan_peer_q_t peer_q;
} wan_conn_t;

static wan_conn_t g_host;
static wan_conn_t g_client;
static int g_curl_inited = 0;
static pthread_mutex_t g_init_mtx = PTHREAD_MUTEX_INITIALIZER;

static void wan_ensure_curl(void)
{
    pthread_mutex_lock(&g_init_mtx);
    if (!g_curl_inited)
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_inited = 1;
    }
    pthread_mutex_unlock(&g_init_mtx);
}

int ttns_wan_available(void)
{
#if LIBCURL_VERSION_NUM >= 0x075600
    return 1;
#else
    return 0;
#endif
}

const char *ttns_wan_last_error(void)
{
    if (g_client.err[0])
        return g_client.err;
    if (g_host.err[0])
        return g_host.err;
    return "";
}

static void wan_sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static int wan_json_get_int(const char *json, const char *key, int *out)
{
    char pat[64];
    const char *p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p)
        return -1;
    p = strchr(p + strlen(pat), ':');
    if (!p)
        return -1;
    p++;
    while (*p == ' ')
        p++;
    *out = atoi(p);
    return 0;
}

static int wan_json_get_str(const char *json, const char *key, char *out, size_t outlen)
{
    char pat[64];
    const char *p;
    const char *end;
    size_t n;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p)
        return -1;
    p = strchr(p + strlen(pat), ':');
    if (!p)
        return -1;
    p = strchr(p, '"');
    if (!p)
        return -1;
    p++;
    end = strchr(p, '"');
    if (!end)
        return -1;
    n = (size_t)(end - p);
    if (n >= outlen)
        n = outlen - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

static int wan_ws_send_text(CURL *curl, const char *text)
{
    size_t sent = 0;
    CURLcode rc = curl_ws_send(curl, text, strlen(text), &sent, 0, CURLWS_TEXT);
    return (rc == CURLE_OK) ? 0 : -1;
}

static int wan_ws_send_bin(CURL *curl, const void *data, size_t len)
{
    size_t sent = 0;
    CURLcode rc = curl_ws_send(curl, data, len, &sent, 0, CURLWS_BINARY);
    return (rc == CURLE_OK) ? 0 : -1;
}

static void *wan_io_thread(void *arg)
{
    wan_conn_t *c = (wan_conn_t *)arg;
    CURLcode rc;
    char hello[192];
    unsigned char rbuf[WAN_PKT_MAX + 64];

    c->curl = curl_easy_init();
    if (!c->curl)
    {
        snprintf(c->err, sizeof(c->err), "curl init failed");
        c->active = 0;
        return NULL;
    }

    curl_easy_setopt(c->curl, CURLOPT_URL, c->url);
    curl_easy_setopt(c->curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(c->curl, CURLOPT_TIMEOUT, 20L);
#if defined(CURLOPT_SSL_VERIFYPEER)
    curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYHOST, 2L);
#endif

    rc = curl_easy_perform(c->curl);
    if (rc != CURLE_OK)
    {
        snprintf(c->err, sizeof(c->err), "WAN connect failed: %s", curl_easy_strerror(rc));
        curl_easy_cleanup(c->curl);
        c->curl = NULL;
        c->active = 0;
        return NULL;
    }

    {
        curl_socket_t sock = CURL_SOCKET_BAD;
        if (curl_easy_getinfo(c->curl, CURLINFO_ACTIVESOCKET, &sock) == CURLE_OK
            && sock != CURL_SOCKET_BAD)
        {
#ifdef _WIN32
            u_long nb = 1;
            ioctlsocket(sock, FIONBIO, &nb);
#else
            int flags = fcntl(sock, F_GETFL, 0);
            if (flags >= 0)
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        }
    }

    if (c->is_host)
        snprintf(hello, sizeof(hello),
                 "{\"type\":\"host\",\"room\":\"%s\",\"name\":\"%s\"}", c->room, c->name);
    else
        snprintf(hello, sizeof(hello),
                 "{\"type\":\"join\",\"room\":\"%s\",\"name\":\"%s\"}", c->room, c->name);

    if (wan_ws_send_text(c->curl, hello) != 0)
    {
        snprintf(c->err, sizeof(c->err), "WAN hello send failed");
        goto done;
    }

    while (c->active)
    {
        size_t nread = 0;
        const struct curl_ws_frame *meta = NULL;

        /* Drain outbound queue */
        {
            int n;
            unsigned char pkt[WAN_PKT_MAX];
            while ((n = wan_q_pop(&c->tx_q, pkt, sizeof(pkt), 0)) > 0)
            {
                if (wan_ws_send_bin(c->curl, pkt, (size_t)n) != 0)
                {
                    snprintf(c->err, sizeof(c->err), "WAN send failed");
                    goto done;
                }
            }
        }

        rc = curl_ws_recv(c->curl, rbuf, sizeof(rbuf), &nread, &meta);
        if (rc == CURLE_AGAIN)
        {
            wan_sleep_ms(5);
            continue;
        }
        if (rc != CURLE_OK)
        {
            snprintf(c->err, sizeof(c->err), "WAN recv failed: %s", curl_easy_strerror(rc));
            break;
        }
        if (!meta || nread < 1)
        {
            wan_sleep_ms(5);
            continue;
        }
        if (meta->flags & CURLWS_CLOSE)
            break;
        if (meta->flags & CURLWS_TEXT)
        {
            char msg[512];
            char typ[32];
            if (nread >= sizeof(msg))
                nread = sizeof(msg) - 1;
            memcpy(msg, rbuf, nread);
            msg[nread] = '\0';
            typ[0] = '\0';
            wan_json_get_str(msg, "type", typ, sizeof(typ));
            if (!strcmp(typ, "host_ok") && c->is_host)
            {
                c->joined = 1;
                c->err[0] = '\0';
            }
            else if (!strcmp(typ, "join_ok") && !c->is_host)
            {
                int slot = 0;
                wan_json_get_int(msg, "slot", &slot);
                c->slot = slot;
                c->joined = 1;
                c->err[0] = '\0';
            }
            else if (!strcmp(typ, "peer_joined") && c->is_host)
            {
                int slot = 0;
                char name[TTNS_REMOTE_NAME_MAX];
                name[0] = '\0';
                wan_json_get_int(msg, "slot", &slot);
                wan_json_get_str(msg, "name", name, sizeof(name));
                wan_peer_q_push(&c->peer_q, slot, name);
            }
            else if (!strcmp(typ, "error"))
            {
                char em[120];
                em[0] = '\0';
                wan_json_get_str(msg, "message", em, sizeof(em));
                snprintf(c->err, sizeof(c->err), "%s", em[0] ? em : "WAN error");
                if (!c->joined)
                    break;
            }
            continue;
        }
        if (meta->flags & CURLWS_BINARY)
            wan_q_push(&c->rx_q, rbuf, (int)nread);
    }

done:
    c->active = 0;
    c->joined = 0;
    if (c->curl)
    {
        curl_easy_cleanup(c->curl);
        c->curl = NULL;
    }
    return NULL;
}

static int wan_start(wan_conn_t *c, int is_host, const char *room, const char *name)
{
    const char *url;

    if (!ttns_wan_available())
    {
        snprintf(c->err, sizeof(c->err), "libcurl WebSocket not available");
        return -1;
    }
    if (c->active)
        return 0;

    wan_ensure_curl();
    memset(c, 0, sizeof(*c));
    wan_q_init(&c->rx_q);
    wan_q_init(&c->tx_q);
    wan_peer_q_init(&c->peer_q);
    c->is_host = is_host;
    ttns_rnet_normalize_room(c->room, sizeof(c->room), room);
    snprintf(c->name, sizeof(c->name), "%s", name && name[0] ? name : (is_host ? "Deck" : "Remote"));

    url = getenv("TTNS_WAN_URL");
    if (!url || !url[0])
        url = TTNS_WAN_SIGNAL_URL;
    snprintf(c->url, sizeof(c->url), "%s", url);

    c->active = 1;
    if (pthread_create(&c->thr, NULL, wan_io_thread, c) != 0)
    {
        c->active = 0;
        snprintf(c->err, sizeof(c->err), "WAN thread failed");
        return -1;
    }

    /* Wait briefly for host_ok / join_ok */
    {
        int i;
        for (i = 0; i < 100 && c->active && !c->joined; i++)
            wan_sleep_ms(50);
    }
    if (!c->joined)
    {
        c->active = 0;
        pthread_join(c->thr, NULL);
        if (!c->err[0])
            snprintf(c->err, sizeof(c->err), "WAN join timeout (is wrx.liveencode.com/ttns up?)");
        wan_q_free(&c->rx_q);
        wan_q_free(&c->tx_q);
        wan_peer_q_free(&c->peer_q);
        memset(c, 0, sizeof(*c));
        return -1;
    }
    return 0;
}

static void wan_stop(wan_conn_t *c)
{
    if (!c->active && !c->joined && !c->curl)
        return;
    c->active = 0;
    if (c->thr)
        pthread_join(c->thr, NULL);
    wan_q_free(&c->rx_q);
    wan_q_free(&c->tx_q);
    wan_peer_q_free(&c->peer_q);
    memset(c, 0, sizeof(*c));
}

int ttns_wan_host_start(const char *room_code, const char *display_name)
{
    return wan_start(&g_host, 1, room_code, display_name);
}

void ttns_wan_host_stop(void)
{
    wan_stop(&g_host);
}

int ttns_wan_host_running(void)
{
    return g_host.active && g_host.joined;
}

int ttns_wan_host_accept_peer(int *slot_out, char *name, size_t name_len)
{
    if (!ttns_wan_host_running())
        return 0;
    return wan_peer_q_pop(&g_host.peer_q, slot_out, name, name_len);
}

int ttns_wan_host_send(const void *packet, size_t len)
{
    if (!ttns_wan_host_running())
        return -1;
    return wan_q_push(&g_host.tx_q, packet, (int)len);
}

int ttns_wan_host_recv(void *buf, size_t buflen, int timeout_ms)
{
    if (!g_host.active)
        return -1;
    return wan_q_pop(&g_host.rx_q, buf, buflen, timeout_ms);
}

int ttns_wan_client_join(const char *room_code, const char *display_name, int *slot_out)
{
    if (wan_start(&g_client, 0, room_code, display_name) != 0)
        return -1;
    if (slot_out)
        *slot_out = g_client.slot;
    return 0;
}

void ttns_wan_client_leave(void)
{
    wan_stop(&g_client);
}

int ttns_wan_client_connected(void)
{
    return g_client.active && g_client.joined;
}

int ttns_wan_client_send(const void *packet, size_t len)
{
    if (!ttns_wan_client_connected())
        return -1;
    return wan_q_push(&g_client.tx_q, packet, (int)len);
}

int ttns_wan_client_recv(void *buf, size_t buflen, int timeout_ms)
{
    if (!g_client.active)
        return -1;
    return wan_q_pop(&g_client.rx_q, buf, buflen, timeout_ms);
}

/* ---- core.liveencode.com reachability (telephone LED) ---- */

#ifndef TTNS_CORE_REACH_URL
#define TTNS_CORE_REACH_URL "https://core.liveencode.com/"
#endif

static volatile int g_core_reach = 0;
static pthread_t g_core_reach_thr;
static int g_core_reach_started = 0;

static int core_reach_probe_once(void)
{
    CURL *c;
    CURLcode rc;
    long code = 0;

    wan_ensure_curl();
    c = curl_easy_init();
    if (!c)
        return 0;

    curl_easy_setopt(c, CURLOPT_URL, TTNS_CORE_REACH_URL);
    curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 3L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    rc = curl_easy_perform(c);
    if (rc == CURLE_OK)
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);

    /* Any completed TLS/HTTP exchange counts as reachable (even 404/401). */
    (void)code;
    return (rc == CURLE_OK) ? 1 : 0;
}

static void *core_reach_thread(void *arg)
{
    (void)arg;
    for (;;)
    {
        g_core_reach = core_reach_probe_once();
#ifdef _WIN32
        Sleep(10000);
#else
        sleep(10);
#endif
    }
    return NULL;
}

int ttns_core_reach_get(void)
{
    return g_core_reach ? 1 : 0;
}

void ttns_core_reach_start(void)
{
    if (g_core_reach_started)
        return;
    g_core_reach_started = 1;
    if (pthread_create(&g_core_reach_thr, NULL, core_reach_thread, NULL) != 0)
    {
        g_core_reach_started = 0;
        return;
    }
    pthread_detach(g_core_reach_thr);
}
