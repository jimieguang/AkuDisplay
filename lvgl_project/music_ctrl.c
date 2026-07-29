/* ======================================================================
 * music_ctrl.c - FlipPanel Bridge WebSocket client (see music_ctrl.h)
 *
 * Design notes:
 *   - One background pthread owns all networking. The UI never blocks.
 *   - Discovery: bind UDP :50570, read the bridge's broadcast JSON, pull
 *     out "hostAddress" + "hostPort" (falls back to the datagram source IP).
 *   - WebSocket: minimal RFC6455 client. We do the HTTP Upgrade handshake
 *     with a random Sec-WebSocket-Key (we do NOT validate the server's
 *     Accept - a permissive embedded controller is fine here), then speak
 *     masked text frames outbound and parse unmasked frames inbound.
 *   - Commands are queued from the UI thread into a tiny ring buffer and
 *     flushed by the worker. Status pushes update a mutex-guarded snapshot.
 * ====================================================================== */
#include "music_ctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define DISCOVERY_PORT 50570
#define WS_PATH        "/ws"
#define CMD_QUEUE_LEN  8
#define ACTION_MAXLEN  32

/* ---- shared state (guarded by g_lock) ---- */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static mc_status_t     g_status;                 /* current now-playing snapshot */
static char            g_queue[CMD_QUEUE_LEN][ACTION_MAXLEN];
static int             g_q_head, g_q_tail;       /* ring buffer indices */
static int             g_started;                /* init guard */
static volatile int    g_stop;                   /* set by music_ctrl_shutdown */
static volatile unsigned g_gen;                  /* bumps on every status update */

/* ====================================================================== */
/* small utilities                                                        */
/* ====================================================================== */

static void status_set_connected(int on, const char *device)
{
    pthread_mutex_lock(&g_lock);
    g_status.connected = on;
    if (!on) {
        g_status.title[0] = g_status.artist[0] = g_status.state[0] = '\0';
    } else if (device) {
        snprintf(g_status.device, sizeof g_status.device, "%s", device);
    }
    pthread_mutex_unlock(&g_lock);
}

/* pop the oldest queued action into buf; returns 1 if one was dequeued */
static int queue_pop(char *buf, int sz)
{
    int got = 0;
    pthread_mutex_lock(&g_lock);
    if (g_q_head != g_q_tail) {
        snprintf(buf, sz, "%s", g_queue[g_q_head]);
        g_q_head = (g_q_head + 1) % CMD_QUEUE_LEN;
        got = 1;
    }
    pthread_mutex_unlock(&g_lock);
    return got;
}

/* base64 of a byte buffer (used for Sec-WebSocket-Key) */
static void b64(const unsigned char *in, int n, char *out)
{
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, o = 0;
    for (i = 0; i < n; i += 3) {
        int b0 = in[i];
        int b1 = (i + 1 < n) ? in[i + 1] : 0;
        int b2 = (i + 2 < n) ? in[i + 2] : 0;
        out[o++] = t[b0 >> 2];
        out[o++] = t[((b0 & 3) << 4) | (b1 >> 4)];
        out[o++] = (i + 1 < n) ? t[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out[o++] = (i + 2 < n) ? t[b2 & 63] : '=';
    }
    out[o] = '\0';
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* Unescape a JSON string body (between the quotes) into UTF-8. Handles
 * \" \\ \/ \n \t and \uXXXX (BMP). Non-ASCII \u sequences become UTF-8 -
 * they will only render if the font has the glyph (montserrat is Latin
 * only, so CJK titles show as placeholder boxes; that is acceptable). */
static void json_unescape(const char *s, int n, char *out, int osz)
{
    int i, o = 0;
    for (i = 0; i < n && o < osz - 1; i++) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            if (c == 'u' && i + 4 < n) {
                int cp = (hex_val(s[i+1]) << 12) | (hex_val(s[i+2]) << 8) |
                         (hex_val(s[i+3]) << 4) | hex_val(s[i+4]);
                i += 4;
                if (cp < 0x80) {
                    out[o++] = (char)cp;
                } else if (cp < 0x800) {
                    if (o < osz - 2) {
                        out[o++] = (char)(0xC0 | (cp >> 6));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                } else {
                    if (o < osz - 3) {
                        out[o++] = (char)(0xE0 | (cp >> 12));
                        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                }
            } else if (c == 'n') { out[o++] = '\n'; }
            else if (c == 't')   { out[o++] = '\t'; }
            else                 { out[o++] = c; }
        } else {
            out[o++] = s[i];
        }
    }
    out[o] = '\0';
}

/* Extract a string value for "key" from a JSON blob. Returns 1 on success
 * (out filled, may be empty for null), 0 if key not present. */
static int json_get_str(const char *json, const char *key, char *out, int osz)
{
    char needle[48];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) { return 0; }
    p += strlen(needle);
    while (*p && *p != ':') { p++; }
    if (*p != ':') { return 0; }
    p++;
    while (*p == ' ' || *p == '\t') { p++; }
    if (*p == '"') {
        p++;
        const char *start = p;
        while (*p && !(*p == '"' && *(p - 1) != '\\')) { p++; }
        json_unescape(start, (int)(p - start), out, osz);
    } else {
        out[0] = '\0';   /* null / non-string */
    }
    return 1;
}

/* Extract an integer value for "key". Returns 1 and sets *out on success. */
static int json_get_int(const char *json, const char *key, int *out)
{
    char needle[48];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) { return 0; }
    p += strlen(needle);
    while (*p && *p != ':') { p++; }
    if (*p != ':') { return 0; }
    p++;
    while (*p == ' ' || *p == '\t') { p++; }
    *out = atoi(p);
    return 1;
}

/* ====================================================================== */
/* socket helpers                                                         */
/* ====================================================================== */

static int write_all(int fd, const void *buf, int len)
{
    const char *p = buf;
    int left = len;
    while (left > 0) {
        int n = send(fd, p, left, MSG_NOSIGNAL);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR)) { continue; }
            return -1;
        }
        p += n; left -= n;
    }
    return 0;
}

/* Read exactly n bytes with an overall timeout (ms). <=0 on error/timeout. */
static int read_n(int fd, void *buf, int n, int timeout_ms)
{
    char *p = buf;
    int left = n;
    while (left > 0) {
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(fd, &rf);
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        int r = select(fd + 1, &rf, NULL, NULL, &tv);
        if (r <= 0) { return -1; }
        int g = recv(fd, p, left, 0);
        if (g <= 0) {
            if (g < 0 && errno == EINTR) { continue; }
            return -1;
        }
        p += g; left -= g;
    }
    return n;
}

/* Non-blocking connect with timeout. Returns fd or -1. */
static int tcp_connect(const char *ip, int port, int timeout_ms)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) { close(fd); return -1; }

    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);

    int r = connect(fd, (struct sockaddr *)&sa, sizeof sa);
    if (r < 0 && errno == EINPROGRESS) {
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(fd, &wf);
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        if (select(fd + 1, NULL, &wf, NULL, &tv) <= 0) { close(fd); return -1; }
        int err = 0; socklen_t el = sizeof err;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el);
        if (err != 0) { close(fd); return -1; }
    } else if (r < 0) {
        close(fd); return -1;
    }

    fcntl(fd, F_SETFL, fl);                 /* back to blocking */
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    return fd;
}

/* ====================================================================== */
/* WebSocket framing                                                      */
/* ====================================================================== */

/* Send a masked text frame (client frames MUST be masked per RFC6455). */
static int ws_send_text(int fd, const char *msg)
{
    size_t n = strlen(msg);
    unsigned char mask[4];
    for (int i = 0; i < 4; i++) { mask[i] = (unsigned char)(rand() & 0xFF); }

    unsigned char hdr[8];
    int h = 0;
    hdr[0] = 0x81;                          /* FIN + text opcode */
    if (n < 126) {
        hdr[1] = 0x80 | (unsigned char)n;   /* mask bit + len */
        h = 2;
    } else if (n < 65536) {
        hdr[1] = 0x80 | 126;
        hdr[2] = (unsigned char)((n >> 8) & 0xFF);
        hdr[3] = (unsigned char)(n & 0xFF);
        h = 4;
    } else {
        return -1;                          /* commands are tiny; never hit */
    }
    memcpy(hdr + h, mask, 4); h += 4;
    if (write_all(fd, hdr, h) < 0) { return -1; }

    unsigned char buf[256];
    size_t i = 0;
    while (i < n) {
        size_t chunk = n - i;
        if (chunk > sizeof buf) { chunk = sizeof buf; }
        for (size_t k = 0; k < chunk; k++) {
            buf[k] = (unsigned char)msg[i + k] ^ mask[(i + k) & 3];
        }
        if (write_all(fd, buf, (int)chunk) < 0) { return -1; }
        i += chunk;
    }
    return 0;
}

/* Send a control frame (opcode) with optional payload, masked. */
static int ws_send_ctrl(int fd, unsigned char opcode, const unsigned char *pl, int n)
{
    unsigned char mask[4];
    for (int i = 0; i < 4; i++) { mask[i] = (unsigned char)(rand() & 0xFF); }
    unsigned char frame[4 + 125];
    if (n > 125) { n = 125; }
    frame[0] = 0x80 | opcode;
    frame[1] = 0x80 | (unsigned char)n;
    memcpy(frame + 2, mask, 4);
    for (int k = 0; k < n; k++) { frame[6 + k] = (pl ? pl[k] : 0) ^ mask[k & 3]; }
    return write_all(fd, frame, 6 + n);
}

/* Receive one frame. Fills *opcode and payload (NUL-terminated). Returns
 * payload length, or -1 on error/timeout/close. */
static int ws_recv(int fd, int *opcode, char *out, int maxlen, int timeout_ms)
{
    unsigned char h2[2];
    if (read_n(fd, h2, 2, timeout_ms) <= 0) { return -1; }
    *opcode = h2[0] & 0x0F;
    int masked = h2[1] & 0x80;
    uint64_t len = h2[1] & 0x7F;

    if (len == 126) {
        unsigned char e[2];
        if (read_n(fd, e, 2, timeout_ms) <= 0) { return -1; }
        len = ((uint64_t)e[0] << 8) | e[1];
    } else if (len == 127) {
        unsigned char e[8];
        if (read_n(fd, e, 8, timeout_ms) <= 0) { return -1; }
        len = 0;
        for (int i = 0; i < 8; i++) { len = (len << 8) | e[i]; }
    }

    unsigned char mkey[4] = { 0, 0, 0, 0 };
    if (masked) {
        if (read_n(fd, mkey, 4, timeout_ms) <= 0) { return -1; }
    }

    uint64_t pos = 0, left = len;
    int oi = 0;
    while (left > 0) {
        unsigned char b[512];
        int chunk = (left > sizeof b) ? (int)sizeof b : (int)left;
        if (read_n(fd, b, chunk, timeout_ms) <= 0) { return -1; }
        for (int k = 0; k < chunk; k++) {
            unsigned char v = masked ? (b[k] ^ mkey[pos & 3]) : b[k];
            if (oi < maxlen - 1) { out[oi++] = (char)v; }
            pos++;
        }
        left -= chunk;
    }
    out[oi] = '\0';
    return oi;
}

/* Perform the HTTP Upgrade handshake. Returns 0 on HTTP 101. */
static int ws_handshake(int fd, const char *host, int port)
{
    unsigned char rnd[16];
    for (int i = 0; i < 16; i++) { rnd[i] = (unsigned char)(rand() & 0xFF); }
    char key[32];
    b64(rnd, 16, key);

    char req[320];
    int n = snprintf(req, sizeof req,
        "GET " WS_PATH " HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n", host, port, key);
    if (write_all(fd, req, n) < 0) { return -1; }

    /* read response headers up to the blank line */
    char resp[512];
    int total = 0;
    while (total < (int)sizeof resp - 1) {
        char c;
        if (read_n(fd, &c, 1, 4000) <= 0) { return -1; }
        resp[total++] = c;
        if (total >= 4 && resp[total-4]=='\r' && resp[total-3]=='\n'
                       && resp[total-2]=='\r' && resp[total-1]=='\n') {
            break;
        }
    }
    resp[total] = '\0';
    return (strstr(resp, " 101") != NULL) ? 0 : -1;
}

/* ====================================================================== */
/* discovery                                                              */
/* ====================================================================== */

/* Listen for a bridge broadcast on UDP :50570. On success fills host/port
 * (and device) and returns 1; returns 0 on timeout. */
static int discover(char *host, int hsz, int *port, char *device, int dsz,
                    int timeout_ms)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { return 0; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port        = htons(DISCOVERY_PORT);
    if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0) { close(fd); return 0; }

    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(fd, &rf);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    if (select(fd + 1, &rf, NULL, NULL, &tv) <= 0) { close(fd); return 0; }

    char buf[1024];
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    int n = recvfrom(fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
    close(fd);
    if (n <= 0) { return 0; }
    buf[n] = '\0';

    int got_port = 0;
    if (!json_get_int(buf, "hostPort", &got_port) || got_port <= 0) {
        got_port = 50571;
    }
    *port = got_port;

    char addr[64] = "";
    if (!json_get_str(buf, "hostAddress", addr, sizeof addr) || addr[0] == '\0') {
        /* fall back to the datagram source address */
        inet_ntop(AF_INET, &from.sin_addr, addr, sizeof addr);
    }
    snprintf(host, hsz, "%s", addr);

    if (device) {
        device[0] = '\0';
        json_get_str(buf, "deviceName", device, dsz);
    }
    return (host[0] != '\0');
}

/* ====================================================================== */
/* worker thread                                                          */
/* ====================================================================== */

static void apply_status(const char *json)
{
    char title[160], artist[160], state[24];
    title[0] = artist[0] = state[0] = '\0';
    json_get_str(json, "musicTitle", title, sizeof title);
    json_get_str(json, "musicArtist", artist, sizeof artist);
    json_get_str(json, "musicPlaybackState", state, sizeof state);

    pthread_mutex_lock(&g_lock);
    snprintf(g_status.title,  sizeof g_status.title,  "%s", title);
    snprintf(g_status.artist, sizeof g_status.artist, "%s", artist);
    snprintf(g_status.state,  sizeof g_status.state,  "%s", state);
    pthread_mutex_unlock(&g_lock);
    g_gen++;   /* notify UI that fresh data is available */
}

/* Run one connected session until the socket drops or g_stop is set. */
static void session(int fd)
{
    char rxbuf[4096];
    while (!g_stop) {
        /* 1. flush any queued commands */
        char action[ACTION_MAXLEN];
        while (queue_pop(action, sizeof action)) {
            char msg[96];
            snprintf(msg, sizeof msg,
                     "{\"messageType\":\"command\",\"actionId\":\"%s\"}", action);
            if (ws_send_text(fd, msg) < 0) { return; }
        }

        /* 2. wait briefly for an inbound frame */
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(fd, &rf);
        struct timeval tv = { 0, 200 * 1000 };   /* 200ms so commands stay snappy */
        int r = select(fd + 1, &rf, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) { continue; } return; }
        if (r == 0) { continue; }

        int opcode = 0;
        int n = ws_recv(fd, &opcode, rxbuf, sizeof rxbuf, 3000);
        if (n < 0) { return; }

        if (opcode == 0x8) {                     /* close */
            return;
        } else if (opcode == 0x9) {              /* ping -> pong */
            ws_send_ctrl(fd, 0xA, (unsigned char *)rxbuf, n);
        } else if (opcode == 0x1) {              /* text: status message */
            if (strstr(rxbuf, "\"status\"") || strstr(rxbuf, "musicPlaybackState")) {
                apply_status(rxbuf);
            }
        }
        /* opcode 0x0 (continuation) / 0xA (pong) ignored */
    }
}

static void *worker(void *arg)
{
    (void)arg;
    while (!g_stop) {
        char host[64] = "", device[64] = "";
        int  port = 0;

        if (!discover(host, sizeof host, &port, device, sizeof device, 4000)) {
            /* no bridge on the LAN yet; back off a little and retry */
            usleep(500 * 1000);
            continue;
        }
        if (g_stop) break;

        int fd = tcp_connect(host, port, 3000);
        if (fd < 0) { usleep(1000 * 1000); continue; }

        if (ws_handshake(fd, host, port) < 0) {
            close(fd);
            usleep(1000 * 1000);
            continue;
        }

        status_set_connected(1, device[0] ? device : host);
        session(fd);
        close(fd);
        status_set_connected(0, NULL);
    }
    return NULL;
}

/* ====================================================================== */
/* public API                                                             */
/* ====================================================================== */

void music_ctrl_init(void)
{
    if (g_started) { return; }
    g_started = 1;
    memset(&g_status, 0, sizeof g_status);
    g_q_head = g_q_tail = 0;

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th, &attr, worker, NULL);
    pthread_attr_destroy(&attr);
}

void music_ctrl_action(const char *action_id)
{
    if (!action_id || !action_id[0]) { return; }
    pthread_mutex_lock(&g_lock);
    int next = (g_q_tail + 1) % CMD_QUEUE_LEN;
    if (next != g_q_head) {                       /* drop if full */
        snprintf(g_queue[g_q_tail], ACTION_MAXLEN, "%s", action_id);
        g_q_tail = next;
    }
    pthread_mutex_unlock(&g_lock);
}

void music_ctrl_get(mc_status_t *out)
{
    if (!out) { return; }
    pthread_mutex_lock(&g_lock);
    *out = g_status;
    pthread_mutex_unlock(&g_lock);
}

unsigned music_ctrl_generation(void)
{
    return g_gen;
}

void music_ctrl_shutdown(void)
{
    if (!g_started) { return; }
    g_stop = 1;
    /* The worker uses select() with <=4s timeout in discover and 200ms in
     * session; wait long enough for it to notice g_stop and exit. */
    usleep(300 * 1000);
}
