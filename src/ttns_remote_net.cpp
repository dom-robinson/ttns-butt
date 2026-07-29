#include "ttns_remote_net.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef _WIN32
 #include <winsock2.h>
 #include <ws2tcpip.h>
 typedef int socklen_t;
 #define CLOSESOCK closesocket
#else
 #include <arpa/inet.h>
 #include <fcntl.h>
 #include <ifaddrs.h>
 #include <net/if.h>
 #include <netinet/in.h>
 #include <netinet/tcp.h>
 #include <sys/select.h>
 #include <sys/socket.h>
 #include <unistd.h>
 #define CLOSESOCK close
#endif

static void ttns_room_normalize(char *dst, size_t dst_len, const char *src)
{
    size_t i = 0;
    if (!dst || dst_len == 0)
        return;
    dst[0] = '\0';
    if (!src)
        return;
    while (*src && isspace((unsigned char)*src))
        src++;
    while (*src && i + 1 < dst_len)
    {
        if (isspace((unsigned char)*src))
            break;
        dst[i++] = (char)toupper((unsigned char)*src++);
    }
    dst[i] = '\0';
}

void ttns_rnet_normalize_room(char *dst, size_t dst_len, const char *src)
{
    ttns_room_normalize(dst, dst_len, src);
}

static uint32_t ttns_magic(void)
{
    return 0x53544E54u; /* T T N S */
}

static int ttns_set_nonblock(int fd, int nb)
{
#ifdef _WIN32
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (nb)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
#endif
}

static int ttns_wait_fd(int fd, int for_write, int timeout_ms)
{
    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (for_write)
        r = select(fd + 1, NULL, &fds, NULL, &tv);
    else
        r = select(fd + 1, &fds, NULL, NULL, &tv);
    return r;
}

int ttns_rnet_build_packet(void *out, size_t out_cap, uint8_t type, uint8_t slot,
                           uint16_t seq, const void *payload, uint16_t len)
{
    unsigned char *buf = (unsigned char *)out;
    ttns_remote_hdr_t *h;
    int total;

    if (!out || out_cap < sizeof(ttns_remote_hdr_t) || len > TTNS_REMOTE_MAX_PACKET)
        return -1;
    total = (int)sizeof(ttns_remote_hdr_t) + (int)len;
    if ((size_t)total > out_cap)
        return -1;
    h = (ttns_remote_hdr_t *)buf;
    h->magic = ttns_magic();
    h->type = type;
    h->slot = slot;
    h->seq = htons(seq);
    h->len = htons(len);
    if (len > 0 && payload)
        memcpy(buf + sizeof(*h), payload, len);
    return total;
}

int ttns_rnet_send_packet(int fd, uint8_t type, uint8_t slot, uint16_t seq,
                          const void *payload, uint16_t len)
{
    unsigned char buf[sizeof(ttns_remote_hdr_t) + TTNS_REMOTE_MAX_PACKET];
    int total;
    int sent = 0;
    int n;

    if (fd < 0 || len > TTNS_REMOTE_MAX_PACKET)
        return -1;

    total = ttns_rnet_build_packet(buf, sizeof(buf), type, slot, seq, payload, len);
    if (total < 0)
        return -1;
    while (sent < total)
    {
        n = (int)send(fd, (char *)buf + sent, (size_t)(total - sent), 0);
        if (n <= 0)
        {
            if (ttns_wait_fd(fd, 1, 1000) <= 0)
                return -1;
            continue;
        }
        sent += n;
    }
    return 0;
}

int ttns_rnet_recv_packet(int fd, ttns_remote_hdr_t *hdr, void *payload,
                          uint16_t max_len, int timeout_ms)
{
    unsigned char buf[sizeof(ttns_remote_hdr_t)];
    int got = 0;
    int n;
    uint16_t plen;

    if (fd < 0 || !hdr)
        return -1;

    while (got < (int)sizeof(buf))
    {
        if (ttns_wait_fd(fd, 0, timeout_ms) <= 0)
            return 0;
        n = (int)recv(fd, (char *)buf + got, sizeof(buf) - (size_t)got, 0);
        if (n == 0)
            return -1;
        if (n < 0)
            continue;
        got += n;
    }

    memcpy(hdr, buf, sizeof(*hdr));
    if (hdr->magic != ttns_magic())
        return -1;
    plen = ntohs(hdr->len);
    hdr->seq = ntohs(hdr->seq);
    hdr->len = plen;

    if (plen > max_len)
        return -1;

    got = 0;
    while (got < (int)plen)
    {
        if (ttns_wait_fd(fd, 0, timeout_ms) <= 0)
            return -1;
        n = (int)recv(fd, (char *)payload + got, (size_t)(plen - got), 0);
        if (n == 0)
            return -1;
        if (n < 0)
            continue;
        got += n;
    }
    return (int)plen;
}

int ttns_rnet_listen(int port)
{
    int fd;
    int yes = 1;
    struct sockaddr_in addr;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        CLOSESOCK(fd);
        return -1;
    }
    if (listen(fd, 4) < 0)
    {
        CLOSESOCK(fd);
        return -1;
    }
    ttns_set_nonblock(fd, 1);
    return fd;
}

int ttns_rnet_accept(int listen_fd, char *peer_ip, int peer_ip_len, int timeout_ms)
{
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int fd;

    if (listen_fd < 0)
        return -1;
    if (ttns_wait_fd(listen_fd, 0, timeout_ms) <= 0)
        return -1;

    fd = (int)accept(listen_fd, (struct sockaddr *)&peer, &plen);
    if (fd < 0)
        return -1;

    {
        int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(yes));
    }
    if (peer_ip && peer_ip_len > 0)
        inet_ntop(AF_INET, &peer.sin_addr, peer_ip, (socklen_t)peer_ip_len);
    return fd;
}

int ttns_rnet_connect(const char *host, int port, int timeout_ms)
{
    int fd;
    struct sockaddr_in addr;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        CLOSESOCK(fd);
        return -1;
    }

    ttns_set_nonblock(fd, 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (errno != EINPROGRESS)
#endif
        {
            /* may still be in progress */
        }
        if (ttns_wait_fd(fd, 1, timeout_ms) <= 0)
        {
            CLOSESOCK(fd);
            return -1;
        }
    }
    ttns_set_nonblock(fd, 0);
    {
        int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(yes));
    }
    return fd;
}

void ttns_rnet_close(int *fd)
{
    if (fd && *fd >= 0)
    {
        CLOSESOCK(*fd);
        *fd = -1;
    }
}

/* ---- discovery ---- */

static pthread_t disc_thread;
static volatile int disc_run = 0;
static char disc_room[16];
static int disc_tcp_port = 0;
static int disc_sock = -1;

static int ttns_room_eq_prefix(const char *payload, int payload_len, const char *room)
{
    size_t rlen = strlen(room);
    size_t i;
    if ((size_t)payload_len < rlen)
        return 0;
    for (i = 0; i < rlen; i++)
    {
        if (toupper((unsigned char)payload[i]) != toupper((unsigned char)room[i]))
            return 0;
    }
    return 1;
}

static void *ttns_discovery_thread(void *arg)
{
    unsigned char buf[64];
    struct sockaddr_in from;
    socklen_t flen;
    int n;

    (void)arg;
    while (disc_run)
    {
        if (disc_sock < 0)
            break;
        if (ttns_wait_fd(disc_sock, 0, 200) <= 0)
            continue;

        flen = sizeof(from);
        n = (int)recvfrom(disc_sock, (char *)buf, sizeof(buf) - 1, 0,
                          (struct sockaddr *)&from, &flen);
        if (n < 8)
            continue;
        /* Query: "TTNS?" + room code */
        if (memcmp(buf, "TTNS?", 5) != 0)
            continue;
        if (!ttns_room_eq_prefix((char *)buf + 5, n - 5, disc_room))
            continue;

        {
            unsigned char reply[32];
            uint16_t p = htons((uint16_t)disc_tcp_port);
            size_t rlen_room = strlen(disc_room);
            int rlen = 5 + (int)rlen_room + 2;
            memcpy(reply, "TTNS!", 5);
            memcpy(reply + 5, disc_room, rlen_room);
            memcpy(reply + 5 + rlen_room, &p, 2);
            sendto(disc_sock, (char *)reply, (size_t)rlen, 0,
                   (struct sockaddr *)&from, flen);
        }
    }
    return NULL;
}

int ttns_rnet_discovery_reply_start(const char *room_code, int tcp_port)
{
    struct sockaddr_in addr;
    int yes = 1;
    char room[16];

    ttns_rnet_discovery_reply_stop();
    ttns_room_normalize(room, sizeof(room), room_code);
    if (!room[0] || tcp_port <= 0)
        return -1;

    snprintf(disc_room, sizeof(disc_room), "%s", room);
    disc_tcp_port = tcp_port;

    disc_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (disc_sock < 0)
        return -1;
    setsockopt(disc_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(disc_sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes, sizeof(yes));
#endif
    setsockopt(disc_sock, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(TTNS_REMOTE_UDP_PORT);
    if (bind(disc_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        CLOSESOCK(disc_sock);
        disc_sock = -1;
        return -1;
    }

    disc_run = 1;
    if (pthread_create(&disc_thread, NULL, ttns_discovery_thread, NULL) != 0)
    {
        disc_run = 0;
        CLOSESOCK(disc_sock);
        disc_sock = -1;
        return -1;
    }
    return 0;
}

void ttns_rnet_discovery_reply_stop(void)
{
    if (disc_run)
    {
        disc_run = 0;
        if (disc_sock >= 0)
        {
            CLOSESOCK(disc_sock);
            disc_sock = -1;
        }
        pthread_join(disc_thread, NULL);
    }
    else if (disc_sock >= 0)
    {
        CLOSESOCK(disc_sock);
        disc_sock = -1;
    }
}

/* macOS often does not loop 255.255.255.255 back to local sockets — also probe
 * 127.0.0.1, each iface unicast, and each iface broadcast. */
static void ttns_discovery_send_all(int fd, const unsigned char *query, int qlen)
{
    struct sockaddr_in dest;

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(TTNS_REMOTE_UDP_PORT);

    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(fd, (const char *)query, (size_t)qlen, 0, (struct sockaddr *)&dest, sizeof(dest));

    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(fd, (const char *)query, (size_t)qlen, 0, (struct sockaddr *)&dest, sizeof(dest));

#ifndef _WIN32
    {
        struct ifaddrs *ifa_list = NULL;
        struct ifaddrs *ifa;

        if (getifaddrs(&ifa_list) == 0)
        {
            for (ifa = ifa_list; ifa; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;
                if (!(ifa->ifa_flags & IFF_UP))
                    continue;
                dest.sin_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                sendto(fd, (const char *)query, (size_t)qlen, 0,
                       (struct sockaddr *)&dest, sizeof(dest));
                if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr
                    && ifa->ifa_broadaddr->sa_family == AF_INET)
                {
                    dest.sin_addr =
                        ((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr;
                    sendto(fd, (const char *)query, (size_t)qlen, 0,
                           (struct sockaddr *)&dest, sizeof(dest));
                }
            }
            freeifaddrs(ifa_list);
        }
    }
#endif
}

int ttns_rnet_discover_room(const char *room_code, char *host, int host_len,
                            int *tcp_port, int timeout_ms)
{
    int fd;
    int yes = 1;
    struct sockaddr_in from;
    struct sockaddr_in bind_addr;
    socklen_t flen;
    unsigned char query[32];
    unsigned char reply[64];
    char room[16];
    size_t rlen;
    int qlen;
    int n;
    int elapsed = 0;

    if (!room_code || !host || host_len < 8 || !tcp_port)
        return -1;

    ttns_room_normalize(room, sizeof(room), room_code);
    rlen = strlen(room);
    if (rlen < 4)
        return -1;

#ifdef _WIN32
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
#endif

    fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes));

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
    {
        CLOSESOCK(fd);
        return -1;
    }
    ttns_set_nonblock(fd, 1);

    qlen = 5 + (int)rlen;
    memcpy(query, "TTNS?", 5);
    memcpy(query + 5, room, rlen);

    while (elapsed < timeout_ms)
    {
        ttns_discovery_send_all(fd, query, qlen);
        if (ttns_wait_fd(fd, 0, 200) > 0)
        {
            flen = sizeof(from);
            n = (int)recvfrom(fd, (char *)reply, sizeof(reply), 0,
                              (struct sockaddr *)&from, &flen);
            if (n >= 5 + (int)rlen + 2
                && memcmp(reply, "TTNS!", 5) == 0
                && ttns_room_eq_prefix((char *)reply + 5, n - 5, room))
            {
                uint16_t p;
                memcpy(&p, reply + 5 + rlen, 2);
                *tcp_port = (int)ntohs(p);
                /* Prefer a routable peer address when the reply came via loopback
                 * only if from is loopback — still fine for same-host TCP. */
                inet_ntop(AF_INET, &from.sin_addr, host, (socklen_t)host_len);
                CLOSESOCK(fd);
                return 0;
            }
        }
        elapsed += 200;
    }

    CLOSESOCK(fd);
    return -1;
}
