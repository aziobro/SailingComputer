#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "config.h"
#include "webui.h"
#include "um982.h"
#include "ble_nmea.h"

#include "certs.h"
#include "mbedtls/base64.h"

// ── Utility macros ────────────────────────────────────────────────────────────

static const char *TAG = "SailComp";

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define DEG_TO_RAD (M_PI / 180.0f)
#define RAD_TO_DEG (180.0f / M_PI)

static inline float fclamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// millis() equivalent — microseconds since boot divided by 1000
static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// ── UART port assignments ─────────────────────────────────────────────────────
// UART1 → UM982 COM1 (NMEA):  GPIO16=RX, GPIO17=TX  @ 115200
// UART2 → UM982 COM2 (RTCM):  GPIO19=RX, GPIO18=TX  @ 115200
// Both are remapped via the GPIO matrix (driver-level pin_cfg), not native.
// UART0 is reserved for USB/debug (Serial0).
#define UART_NMEA  UART_NUM_1   // UM982 COM1 — NMEA in  (GPIO16=RX, GPIO17=TX)
#define UART_RTCM  UART_NUM_2   // UM982 COM2 — RTCM out (GPIO19=RX, GPIO18=TX)

// ── State ─────────────────────────────────────────────────────────────────────

ConfigManager cfgMgr;

struct NmeaMessage {
    char line[256];
};

static int   fixQuality = 0;
static int   satCount   = 0;
static float latitude   = 0, longitude = 0, heading = 0, sog = 0, cog = 0;
static float hdop = 0, altitude = 0;
static float roll = 0;          // heel: pitch of athwartships baseline
static float cogFiltered    = 0;
static bool  cogInitialized = false;
static bool  hdtValid       = false;
static bool  rollValid      = false;

static float leewayAngle        = 0;
static float lateralDrift       = 0;
static float driveSpeed         = 0;
static bool  sailingMetricsValid = false;

// Diagnostic counters
static uint32_t nmeaBytesRx       = 0;  // raw bytes received from UART1 — 0 means UART broken
static uint32_t nmeaLinesRx       = 0;  // complete NMEA sentences parsed
static uint32_t nmeaOverflowDrops = 0;
static uint32_t nmeaParseDrops    = 0;
static uint32_t nmeaOutputDrops   = 0;
static uint32_t nmeaTcpDrops      = 0;

// WiFi
static bool staConnected = false;

// NMEA TCP server
#define MAX_NMEA_CLIENTS 4
static int nmeaSrvFd = -1;
static int nmeaClientFds[MAX_NMEA_CLIENTS];
static SemaphoreHandle_t nmeaClientMtx = NULL;
static QueueHandle_t nmeaParseQueue = NULL;
static QueueHandle_t nmeaOutputQueue = NULL;
static SemaphoreHandle_t telemetryMtx = NULL;
static SemaphoreHandle_t uartNmeaMtx = NULL;

// NTRIP
static int      ntripSock        = -1;
static bool     ntripConnected   = false;
static uint32_t ntripLastAttempt = 0;
static uint32_t ntripBytesIn     = 0;
static uint32_t ntripConnectTime = 0;
static uint32_t ntripSessionBytes = 0;
static int      ntripActiveIdx   = 0;
static int      ntripFailCount   = 0;
static bool     ntripAnyEnabled  = false;

// HTTP server handle
static httpd_handle_t webServer = NULL;

// ── Sailing metrics ───────────────────────────────────────────────────────────

static void updateSailingMetrics() {
    if (!hdtValid || !cogInitialized || sog < cfgMgr.cfg.cogMinSog) {
        sailingMetricsValid = false;
        return;
    }
    float diff = cogFiltered - heading;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    leewayAngle  = diff;
    float rad    = leewayAngle * DEG_TO_RAD;
    lateralDrift = sog * sinf(rad);
    driveSpeed   = sog * cosf(rad);
    sailingMetricsValid = true;
}

static float applyHeadingOffset(float h) {
    h += cfgMgr.cfg.headingOffset;
    while (h >= 360.0f) h -= 360.0f;
    while (h <    0.0f) h += 360.0f;
    return h;
}

// ── Base64 ────────────────────────────────────────────────────────────────────

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode src (len bytes) into dst (must be >= len*4/3 + 4 bytes).
static void base64Encode(const char *src, size_t len, char *dst) {
    size_t i = 0, j = 0;
    uint8_t buf3[3], buf4[4];
    size_t rem = len;
    while (rem--) {
        buf3[i++] = (uint8_t)*src++;
        if (i == 3) {
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;
            for (int k = 0; k < 4; k++) dst[j++] = b64chars[buf4[k]];
            i = 0;
        }
    }
    if (i) {
        for (size_t k = i; k < 3; k++) buf3[k] = 0;
        buf4[0] = (buf3[0] & 0xfc) >> 2;
        buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
        buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
        for (size_t k = 0; k < i + 1; k++) dst[j++] = b64chars[buf4[k]];
        while (i++ < 3) dst[j++] = '=';
    }
    dst[j] = '\0';
}

// ── URL decode / form parsing ─────────────────────────────────────────────────

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static void urlDecode(char *dst, const char *src, size_t dst_size) {
    size_t i = 0;
    while (*src && i < dst_size - 1) {
        if (*src == '+') {
            dst[i++] = ' '; src++;
        } else if (*src == '%' && src[1] && src[2]) {
            dst[i++] = (char)((hexVal(src[1]) << 4) | hexVal(src[2]));
            src += 3;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

static bool appendFmt(char *dst, size_t dst_size, size_t *off,
                      const char *fmt, ...) {
    if (*off >= dst_size) return false;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst + *off, dst_size - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return false;
    if ((size_t)n >= dst_size - *off) {
        *off = dst_size - 1;
        dst[*off] = '\0';
        return false;
    }
    *off += (size_t)n;
    return true;
}

static bool appendJsonString(char *dst, size_t dst_size, size_t *off,
                             const char *s) {
    if (!appendFmt(dst, dst_size, off, "\"")) return false;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        switch (c) {
            case '\\': if (!appendFmt(dst, dst_size, off, "\\\\")) return false; break;
            case '"':  if (!appendFmt(dst, dst_size, off, "\\\"")) return false; break;
            case '\b': if (!appendFmt(dst, dst_size, off, "\\b")) return false; break;
            case '\f': if (!appendFmt(dst, dst_size, off, "\\f")) return false; break;
            case '\n': if (!appendFmt(dst, dst_size, off, "\\n")) return false; break;
            case '\r': if (!appendFmt(dst, dst_size, off, "\\r")) return false; break;
            case '\t': if (!appendFmt(dst, dst_size, off, "\\t")) return false; break;
            default:
                if (c < 0x20) {
                    if (!appendFmt(dst, dst_size, off, "\\u%04x", c)) return false;
                } else {
                    if (*off + 1 >= dst_size) return false;
                    dst[(*off)++] = (char)c;
                    dst[*off] = '\0';
                }
                break;
        }
    }
    return appendFmt(dst, dst_size, off, "\"");
}

// Find key in "k1=v1&k2=v2" body, URL-decode value into val.
// Returns true if key found.
static bool formGet(const char *body, const char *key,
                    char *val, size_t val_size) {
    char search[68];
    snprintf(search, sizeof(search), "%s=", key);
    const char *p = body;
    while ((p = strstr(p, search)) != NULL) {
        if (p == body || *(p - 1) == '&') break;   // ensure full key match
        p++;
    }
    if (!p) { if (val_size) val[0] = '\0'; return false; }
    p += strlen(search);
    const char *end = strchr(p, '&');
    size_t raw_len  = end ? (size_t)(end - p) : strlen(p);
    char raw[512];
    if (raw_len >= sizeof(raw)) raw_len = sizeof(raw) - 1;
    memcpy(raw, p, raw_len); raw[raw_len] = '\0';
    urlDecode(val, raw, val_size);
    return true;
}

// ── NMEA helpers ──────────────────────────────────────────────────────────────

static float nmeaToDeg(float val) {
    int   deg = (int)(val / 100);
    float min = val - deg * 100.0f;
    return deg + min / 60.0f;
}

static void parseGGA(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *tok = strtok(buf, ",");
    int field = 0;
    char latHemi = 'N', lonHemi = 'E';
    while (tok) {
        field++;
        switch (field) {
            case 3:  latitude   = nmeaToDeg(atof(tok)); break;
            case 4:  latHemi    = tok[0]; break;
            case 5:  longitude  = nmeaToDeg(atof(tok)); break;
            case 6:  lonHemi    = tok[0]; break;
            case 7:  fixQuality = atoi(tok); break;
            case 8:  satCount   = atoi(tok); break;
            case 9:  hdop       = atof(tok); break;
            case 10: altitude   = atof(tok); break;
        }
        tok = strtok(NULL, ",");
    }
    if (latHemi == 'S') latitude  = -latitude;
    if (lonHemi == 'W') longitude = -longitude;
}

static void parseHDT(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *tok = strtok(buf, ",");
    int field = 0;
    while (tok) {
        field++;
        if (field == 2 && strlen(tok) > 0) {
            heading  = applyHeadingOffset(atof(tok));
            hdtValid = true;
            updateSailingMetrics();
        }
        tok = strtok(NULL, ",");
    }
}

static void updateCOG(float rawCog, float speedKts) {
    if (speedKts < cfgMgr.cfg.cogMinSog) return;
    if (!cogInitialized) {
        cogFiltered = rawCog; cogInitialized = true; return;
    }
    float minSog = cfgMgr.cfg.cogMinSog;
    float t      = (speedKts - minSog) / (COG_FAST_SOG_KTS - minSog);
    float alpha  = COG_ALPHA_MIN + fclamp(t, 0.0f, 1.0f) * (COG_ALPHA_MAX - COG_ALPHA_MIN);
    float rawRad  = rawCog      * DEG_TO_RAD;
    float filtRad = cogFiltered * DEG_TO_RAD;
    float sinB    = alpha * sinf(rawRad)  + (1.0f - alpha) * sinf(filtRad);
    float cosB    = alpha * cosf(rawRad)  + (1.0f - alpha) * cosf(filtRad);
    cogFiltered   = atan2f(sinB, cosB) * RAD_TO_DEG;
    if (cogFiltered < 0.0f) cogFiltered += 360.0f;
}

static void parseVTG(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *tok = strtok(buf, ",");
    int field = 0; float rawCog = cog;
    while (tok) {
        field++;
        if (field == 2) rawCog = atof(tok);
        if (field == 6) sog    = atof(tok);
        tok = strtok(NULL, ",");
    }
    cog = rawCog;
    updateCOG(rawCog, sog);
    updateSailingMetrics();
}

static void parseHEADINGA(const char *line) {
    const char *data = strchr(line, ';');
    if (!data) return;
    data++;
    char buf[256]; strlcpy(buf, data, sizeof(buf));
    char *star = strchr(buf, '*'); if (star) *star = '\0';
    char *tok = strtok(buf, ",");
    int field = 0;
    while (tok) {
        field++;
        switch (field) {
            case 1:
                if (strncmp(tok, "SOL_COMPUTED", 12) != 0) { rollValid = false; return; }
                break;
            case 4:
                heading  = applyHeadingOffset(atof(tok));
                hdtValid = true;
                break;
            case 5:
                roll      = atof(tok);
                rollValid = true;
                break;
        }
        tok = strtok(NULL, ",");
    }
}

static void processNmeaLine(const char *line) {
    if      (strncmp(line, "$GNGGA",    6) == 0 || strncmp(line, "$GPGGA",    6) == 0) parseGGA(line);
    else if (strncmp(line, "$GNHDT",    6) == 0 || strncmp(line, "$GPHDT",    6) == 0) parseHDT(line);
    else if (strncmp(line, "$GNVTG",    6) == 0 || strncmp(line, "$GPVTG",    6) == 0) parseVTG(line);
    else if (strncmp(line, "#HEADINGA", 9) == 0)                                        parseHEADINGA(line);
}

// ── NMEA checksum ─────────────────────────────────────────────────────────────

static uint8_t nmeaChecksum(const char *s) {
    uint8_t cs = 0;
    if (*s == '$') s++;
    while (*s && *s != '*') cs ^= (uint8_t)*s++;
    return cs;
}

static void enqueueNmeaOutput(const char *line) {
    if (!nmeaOutputQueue) return;
    NmeaMessage msg = {};
    strlcpy(msg.line, line, sizeof(msg.line));
    if (xQueueSend(nmeaOutputQueue, &msg, 0) != pdTRUE) {
        nmeaOutputDrops++;
    }
}

// ── NMEA TCP broadcast ────────────────────────────────────────────────────────

static void broadcastNmea(const char *line) {
    xSemaphoreTake(nmeaClientMtx, portMAX_DELAY);
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (nmeaClientFds[i] < 0) continue;
        int rc = send(nmeaClientFds[i], line, strlen(line), MSG_DONTWAIT);
        if (rc >= 0) rc = send(nmeaClientFds[i], "\r\n", 2, MSG_DONTWAIT);
        if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            nmeaTcpDrops++;
            continue;
        }
        if (rc < 0) {
            close(nmeaClientFds[i]);
            nmeaClientFds[i] = -1;
            ESP_LOGI(TAG, "[NMEA] Client %d gone", i);
        }
    }
    xSemaphoreGive(nmeaClientMtx);
}

static void acceptNmeaClients() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(nmeaSrvFd, &rfds);
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };  // non-blocking poll
    if (select(nmeaSrvFd + 1, &rfds, NULL, NULL, &tv) <= 0) return;
    int fd = accept(nmeaSrvFd, (struct sockaddr *)&client_addr, &addr_len);
    if (fd < 0) return;
    xSemaphoreTake(nmeaClientMtx, portMAX_DELAY);
    bool placed = false;
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (nmeaClientFds[i] < 0) {
            nmeaClientFds[i] = fd;
            placed = true;
            ESP_LOGI(TAG, "[NMEA] Client %d connected: " IPSTR,
                     i, IP2STR((ip4_addr_t *)&client_addr.sin_addr));
            break;
        }
    }
    xSemaphoreGive(nmeaClientMtx);
    if (!placed) close(fd);
}

// ── NTRIP ─────────────────────────────────────────────────────────────────────

static void ntripUpdateEnabled() {
    ntripAnyEnabled = false;
    ntripActiveIdx  = 0;
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        if (cfgMgr.cfg.ntrip[i].enabled && strlen(cfgMgr.cfg.ntrip[i].host) > 0) {
            if (!ntripAnyEnabled) ntripActiveIdx = i;
            ntripAnyEnabled = true;
        }
    }
    if (!ntripAnyEnabled)
        ESP_LOGI(TAG, "[NTRIP] No sources configured");
}

static int ntripNextSource(int idx) {
    for (int i = 1; i <= NTRIP_SOURCES; i++) {
        int next = (idx + i) % NTRIP_SOURCES;
        if (cfgMgr.cfg.ntrip[next].enabled && strlen(cfgMgr.cfg.ntrip[next].host) > 0)
            return next;
    }
    return -1;
}

static void ntripDisconnect() {
    if (ntripSock >= 0) { close(ntripSock); ntripSock = -1; }
    ntripConnected = false;
}

static void ntripSendGGA() {
    int q, sats;
    float latRaw, lonRaw, hdopCopy, altCopy;
    if (telemetryMtx) xSemaphoreTake(telemetryMtx, portMAX_DELAY);
    q = fixQuality;
    sats = satCount;
    latRaw = latitude;
    lonRaw = longitude;
    hdopCopy = hdop;
    altCopy = altitude;
    if (telemetryMtx) xSemaphoreGive(telemetryMtx);

    if (q == 0 || ntripSock < 0) return;
    float lat = fabsf(latRaw),  lon = fabsf(lonRaw);
    int   latDeg = (int)lat;  float latMin = (lat - latDeg) * 60.0f;
    int   lonDeg = (int)lon;  float lonMin = (lon - lonDeg) * 60.0f;
    char body[96];
    snprintf(body, sizeof(body),
             "GPGGA,000000.00,%02d%08.5f,%c,%03d%08.5f,%c,%d,%02d,%.1f,%.1f,M,0.0,M,,",
             latDeg, latMin, latRaw >= 0 ? 'N' : 'S',
             lonDeg, lonMin, lonRaw >= 0 ? 'E' : 'W',
             q, sats, hdopCopy, altCopy);
    char sentence[110];
    snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, nmeaChecksum(body));
    if (send(ntripSock, sentence, strlen(sentence), MSG_DONTWAIT) < 0 &&
        errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGE(TAG, "[NTRIP%d] GGA send error %d", ntripActiveIdx, errno);
        ntripDisconnect();
        ntripFailCount++;
    }
}

// Read one line from socket into buf (strips \r). Returns char count or -1 on error.
static int sockReadLine(int sock, char *buf, size_t len) {
    size_t i = 0;
    while (i < len - 1) {
        char c;
        int r = recv(sock, &c, 1, 0);
        if (r <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

static bool ntripConnect(int idx) {
    NtripSource &src = cfgMgr.cfg.ntrip[idx];
    if (!src.enabled || strlen(src.host) == 0) return false;

    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", src.port);

    if (getaddrinfo(src.host, port_str, &hints, &res) != 0 || !res) {
        ESP_LOGE(TAG, "[NTRIP%d] DNS failed for '%s'", idx, src.host);
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) { freeaddrinfo(res); return false; }

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "[NTRIP%d] Connecting to %s:%d/%s", idx, src.host, src.port, src.mount);
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "[NTRIP%d] TCP connect failed", idx);
        close(sock); freeaddrinfo(res); return false;
    }
    freeaddrinfo(res);

    // Build HTTP request
    char req[768] = "";
    snprintf(req, sizeof(req),
             "GET /%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Ntrip-Version: Ntrip/2.0\r\n"
             "User-Agent: NTRIP SailingComputer/1.0\r\n"
             "Accept: rtk/rtcm\r\n",
             src.mount, src.host);

    if (strlen(src.user) > 0) {
        char creds[192], b64[260];
        snprintf(creds, sizeof(creds), "%s:%s", src.user, src.pass);
        base64Encode(creds, strlen(creds), b64);
        strlcat(req, "Authorization: Basic ", sizeof(req));
        strlcat(req, b64, sizeof(req));
        strlcat(req, "\r\n", sizeof(req));
    }
    strlcat(req, "\r\n", sizeof(req));
    if (send(sock, req, strlen(req), 0) < 0) {
        ESP_LOGE(TAG, "[NTRIP%d] Request send failed", idx);
        close(sock); return false;
    }

    // Read HTTP response headers
    bool got200 = false;
    char line[256];
    for (int tries = 0; tries < 30; tries++) {
        int n = sockReadLine(sock, line, sizeof(line));
        if (n < 0) break;
        ESP_LOGI(TAG, "[NTRIP%d] < %s", idx, line);
        if (!got200 && strstr(line, "200")) got200 = true;
        if (got200 && n <= 0) break;   // blank line = end of headers
    }

    if (!got200) {
        ESP_LOGE(TAG, "[NTRIP%d] No 200 response", idx);
        close(sock); return false;
    }

    ntripSock         = sock;
    ntripConnected    = true;
    ntripFailCount    = 0;
    ntripConnectTime  = millis();
    ntripSessionBytes = 0;
    ESP_LOGI(TAG, "[NTRIP%d] Connected OK", idx);
    ntripSendGGA();
    return true;
}

static void ntripLoop() {
    if (!staConnected) {
        if (ntripConnected) ntripDisconnect();
        return;
    }
    if (!ntripAnyEnabled) return;

    if (!ntripConnected) {
        NtripSource &active = cfgMgr.cfg.ntrip[ntripActiveIdx];
        if (!active.enabled || strlen(active.host) == 0) {
            int next = ntripNextSource(ntripActiveIdx);
            if (next >= 0) { ntripActiveIdx = next; ntripFailCount = 0; ntripLastAttempt = 0; }
            return;
        }
        if (millis() - ntripLastAttempt < NTRIP_RECONNECT_MS) return;
        ntripLastAttempt = millis();

        if (ntripFailCount >= NTRIP_FAILOVER_COUNT) {
            int next = ntripNextSource(ntripActiveIdx);
            if (next >= 0 && next != ntripActiveIdx) {
                ESP_LOGI(TAG, "[NTRIP] Failing over Source%d→Source%d", ntripActiveIdx+1, next+1);
                ntripActiveIdx = next;
            }
            ntripFailCount = 0;
        }
        if (!ntripConnect(ntripActiveIdx)) ntripFailCount++;
        return;
    }

    // Check for server-side close
    {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(ntripSock, &rfds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
        if (select(ntripSock + 1, &rfds, NULL, NULL, &tv) > 0) {
            uint8_t probe;
            int r = recv(ntripSock, &probe, 1, MSG_PEEK);
            if (r == 0) {
                // Server closed connection
                uint32_t sessSecs = (millis() - ntripConnectTime) / 1000;
                ESP_LOGI(TAG, "[NTRIP%d] Server closed after %us / %u bytes",
                         ntripActiveIdx, sessSecs, ntripSessionBytes);
                ntripDisconnect();
                if (ntripSessionBytes > 0) {
                    ntripLastAttempt = 0;   // immediate reconnect
                } else {
                    ntripFailCount++;
                }
                return;
            } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "[NTRIP%d] Socket error %d", ntripActiveIdx, errno);
                ntripDisconnect();
                ntripFailCount++;
                return;
            }
        }
    }

    // Send GGA every 3 seconds
    static uint32_t lastGgaTx = 0;
    if (millis() - lastGgaTx > 3000) { ntripSendGGA(); lastGgaTx = millis(); }

    // Read RTCM and forward to UM982 COM2
    uint8_t buf[256];
    int n = recv(ntripSock, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0) {
        int written = uart_write_bytes(UART_RTCM, (const char *)buf, n);
        if (written < n)
            ESP_LOGW(TAG, "[NTRIP%d] RTCM UART short write %d/%d", ntripActiveIdx, written, n);
        ntripBytesIn      += n;
        ntripSessionBytes += n;
    } else if (n == 0) {
        ntripDisconnect();
        ntripFailCount++;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGE(TAG, "[NTRIP%d] RTCM recv error %d", ntripActiveIdx, errno);
        ntripDisconnect();
        ntripFailCount++;
    }
}

static void ntripTask(void *param) {
    for (;;) {
        ntripLoop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        staConnected = false;
        ESP_LOGI(TAG, "[WiFi] Lost connection — reconnecting");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        staConnected = true;
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "[WiFi] Connected: " IPSTR, IP2STR(&e->ip_info.ip));
        ntripLastAttempt = millis();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "[WiFi] AP client connected");
    }
}

static void startAP() {
    Config &cfg = cfgMgr.cfg;
    esp_netif_create_default_wifi_ap();
    wifi_config_t wcfg = {};
    strlcpy((char *)wcfg.ap.ssid,     cfg.apSSID,     sizeof(wcfg.ap.ssid));
    strlcpy((char *)wcfg.ap.password, cfg.apPassword, sizeof(wcfg.ap.password));
    wcfg.ap.ssid_len       = strlen(cfg.apSSID);
    wcfg.ap.max_connection = 4;
    wcfg.ap.authmode       = strlen(cfg.apPassword) >= 8 ?
                             WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wcfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "[WiFi] AP: %s", cfg.apSSID);
}

static void startSTA() {
    Config &cfg = cfgMgr.cfg;
    esp_netif_create_default_wifi_sta();
    wifi_config_t wcfg = {};
    strlcpy((char *)wcfg.sta.ssid,     cfg.wifiSSID,     sizeof(wcfg.sta.ssid));
    strlcpy((char *)wcfg.sta.password, cfg.wifiPassword, sizeof(wcfg.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(TAG, "[WiFi] Connecting to %s...", cfg.wifiSSID);
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────

// Read full POST body into caller-supplied buf. Returns bytes read, -1 on I/O
// error, or -2 if the caller's fixed buffer is too small.
static int readBody(httpd_req_t *req, char *buf, size_t buf_size) {
    if (buf_size == 0) return -1;
    if (req->content_len >= (int)buf_size) {
        buf[0] = '\0';
        return -2;
    }
    int total = 0, remaining = req->content_len;
    while (remaining > 0) {
        int to_read = MIN(remaining, (int)(buf_size - 1 - total));
        if (to_read <= 0) break;
        int r = httpd_req_recv(req, buf + total, to_read);
        if (r <= 0) return -1;
        total += r; remaining -= r;
    }
    buf[total] = '\0';
    return total;
}

// Get current STA IP as string (or "" if disconnected)
static void getStaIP(char *out, size_t len) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
            snprintf(out, len, IPSTR, IP2STR(&info.ip));
            return;
        }
    }
    strlcpy(out, "0.0.0.0", len);
}

static void getApIP(char *out, size_t len) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
            snprintf(out, len, IPSTR, IP2STR(&info.ip));
            return;
        }
    }
    strlcpy(out, "192.168.4.1", len);
}

// ── Forward declarations ──────────────────────────────────────────────────────
static bool checkAuth(httpd_req_t *req);

// ── HTTP handlers ─────────────────────────────────────────────────────────────

static esp_err_t handleRoot(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, getWebUI(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleStatus(httpd_req_t *req) {
    static const char *fixLabels[] = {
        "No Fix","GPS","DGPS","PPS","RTK Fixed","RTK Float",
        "Dead Reck","Manual","Sim","WAAS"
    };
    char staIP[20], apIP[20];
    getStaIP(staIP, sizeof(staIP));
    getApIP(apIP,   sizeof(apIP));

    char json[1024];
    if (telemetryMtx) xSemaphoreTake(telemetryMtx, portMAX_DELAY);
    snprintf(json, sizeof(json),
        "{"
        "\"fix\":%d,"
        "\"fixLabel\":\"%s\","
        "\"lat\":%.7f,"
        "\"lon\":%.7f,"
        "\"heading\":%.2f,"
        "\"hdtValid\":%s,"
        "\"sog\":%.2f,"
        "\"cog\":%.1f,"
        "\"cogValid\":%s,"
        "\"cogMinSog\":%.2f,"
        "\"sats\":%d,"
        "\"hdop\":%.2f,"
        "\"altitude\":%.2f,"
        "\"roll\":%.2f,"
        "\"rollValid\":%s,"
        "\"leeway\":%.1f,"
        "\"lateralDrift\":%.2f,"
        "\"driveSpeed\":%.2f,"
        "\"sailingValid\":%s,"
        "\"bleEnabled\":%s,"
        "\"bleConnected\":%s,"
        "\"ntripConnected\":%s,"
        "\"ntripActiveIdx\":%d,"
        "\"ntripBytesIn\":%u,"
        "\"nmeaBytesRx\":%u,"
        "\"nmeaLinesRx\":%u,"
        "\"nmeaOverflowDrops\":%u,"
        "\"nmeaParseDrops\":%u,"
        "\"nmeaOutputDrops\":%u,"
        "\"nmeaTcpDrops\":%u,"
        "\"bleDrops\":%u,"
        "\"wifiMode\":\"%s\","
        "\"ip\":\"%s\","
        "\"apIP\":\"%s\""
        "}",
        fixQuality,
        (fixQuality >= 0 && fixQuality <= 9) ? fixLabels[fixQuality] : "Unknown",
        latitude, longitude,
        heading,
        hdtValid             ? "true" : "false",
        sog, cogFiltered,
        (cogInitialized && sog >= cfgMgr.cfg.cogMinSog) ? "true" : "false",
        cfgMgr.cfg.cogMinSog,
        satCount, hdop, altitude, roll,
        rollValid            ? "true" : "false",
        leewayAngle, lateralDrift, driveSpeed,
        sailingMetricsValid  ? "true" : "false",
        bleEnabled           ? "true" : "false",
        bleConnected         ? "true" : "false",
        ntripConnected       ? "true" : "false",
        ntripActiveIdx+1, ntripBytesIn,
        nmeaBytesRx, nmeaLinesRx,
        nmeaOverflowDrops, nmeaParseDrops, nmeaOutputDrops, nmeaTcpDrops,
        bleDropCount,
        cfgMgr.cfg.apMode ? "AP" : "Station",
        staIP, apIP
    );
    if (telemetryMtx) xSemaphoreGive(telemetryMtx);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleGetConfig(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    Config &cfg = cfgMgr.cfg;
    char json[2048] = "";
    size_t off = 0;
    bool ok = appendFmt(json, sizeof(json), &off,
        "{\"apMode\":%s,\"wifiSSID\":", cfg.apMode ? "true" : "false");
    ok = ok && appendJsonString(json, sizeof(json), &off, cfg.wifiSSID);
    ok = ok && appendFmt(json, sizeof(json), &off, ",\"ntrip\":[");
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        if (i) ok = ok && appendFmt(json, sizeof(json), &off, ",");
        ok = ok && appendFmt(json, sizeof(json), &off,
            "{\"enabled\":%s,\"host\":",
            cfg.ntrip[i].enabled ? "true" : "false");
        ok = ok && appendJsonString(json, sizeof(json), &off, cfg.ntrip[i].host);
        ok = ok && appendFmt(json, sizeof(json), &off, ",\"port\":%d,\"mount\":", cfg.ntrip[i].port);
        ok = ok && appendJsonString(json, sizeof(json), &off, cfg.ntrip[i].mount);
        ok = ok && appendFmt(json, sizeof(json), &off, ",\"user\":");
        ok = ok && appendJsonString(json, sizeof(json), &off, cfg.ntrip[i].user);
        ok = ok && appendFmt(json, sizeof(json), &off, "}");
    }
    ok = ok && appendFmt(json, sizeof(json), &off,
        "],\"headingOffset\":%.1f,\"cogMinSog\":%.2f,\"bleNmea\":%s,\"apSSID\":",
        cfg.headingOffset, cfg.cogMinSog,
        cfg.bleNmea ? "true" : "false");
    ok = ok && appendJsonString(json, sizeof(json), &off, cfg.apSSID);
    ok = ok && appendFmt(json, sizeof(json), &off, "}");
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config JSON too large");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleSaveConfig(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[2048] = "";
    int bodyLen = readBody(req, body, sizeof(body));
    if (bodyLen == -2) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_sendstr(req, "Config form too large");
        return ESP_FAIL;
    }
    if (bodyLen < 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    Config &cfg = cfgMgr.cfg;
    char val[192];

    if (formGet(body, "apMode", val, sizeof(val)))
        cfg.apMode = (strcmp(val, "true") == 0);
    if (formGet(body, "wifiSSID", val, sizeof(val)))
        strlcpy(cfg.wifiSSID, val, sizeof(cfg.wifiSSID));
    if (formGet(body, "wifiPassword", val, sizeof(val)) && strlen(val) > 0 &&
        strcmp(val, "(unchanged)") != 0)
        strlcpy(cfg.wifiPassword, val, sizeof(cfg.wifiPassword));

    char key[20];
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        snprintf(key, sizeof(key), "n%denabled", i);
        cfg.ntrip[i].enabled = (formGet(body, key, val, sizeof(val)) &&
                                strcmp(val, "true") == 0);

        snprintf(key, sizeof(key), "n%dhost", i);
        if (formGet(body, key, val, sizeof(val)) && strlen(val) > 0)
            strlcpy(cfg.ntrip[i].host, val, sizeof(cfg.ntrip[i].host));

        snprintf(key, sizeof(key), "n%dport", i);
        if (formGet(body, key, val, sizeof(val)))
            cfg.ntrip[i].port = (uint16_t)atoi(val);

        snprintf(key, sizeof(key), "n%dmount", i);
        if (formGet(body, key, val, sizeof(val)) && strlen(val) > 0)
            strlcpy(cfg.ntrip[i].mount, val, sizeof(cfg.ntrip[i].mount));

        snprintf(key, sizeof(key), "n%duser", i);
        {
            bool ok = formGet(body, key, val, sizeof(val));
            ESP_LOGD(TAG, "[Config] %s: found=%d decoded='%s'", key, ok, ok ? val : "");
            if (ok && strlen(val) > 0)
                strlcpy(cfg.ntrip[i].user, val, sizeof(cfg.ntrip[i].user));
        }

        snprintf(key, sizeof(key), "n%dpass", i);
        {
            bool ok = formGet(body, key, val, sizeof(val));
            if (ok && strlen(val) > 0 && strcmp(val, "(unchanged)") != 0)
                strlcpy(cfg.ntrip[i].pass, val, sizeof(cfg.ntrip[i].pass));
        }
    }

    if (formGet(body, "headingOffset", val, sizeof(val))) cfg.headingOffset = atof(val);
    if (formGet(body, "cogMinSog",     val, sizeof(val))) cfg.cogMinSog     = atof(val);
    if (formGet(body, "bleNmea",       val, sizeof(val)))
        cfg.bleNmea = (strcmp(val, "true") == 0 || strcmp(val, "on") == 0);
    else
        cfg.bleNmea = false;
    if (formGet(body, "apSSID",    val, sizeof(val)) && strlen(val) > 0)
        strlcpy(cfg.apSSID, val, sizeof(cfg.apSSID));
    if (formGet(body, "apPassword", val, sizeof(val)) && strlen(val) > 0 &&
        strcmp(val, "(unchanged)") != 0)
        strlcpy(cfg.apPassword, val, sizeof(cfg.apPassword));
    if (formGet(body, "adminPassword", val, sizeof(val)) && strlen(val) > 0)
        strlcpy(cfg.adminPassword, val, sizeof(cfg.adminPassword));

    cfgMgr.save();
    ntripUpdateEnabled();
    ntripActiveIdx = 0; ntripFailCount = 0;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

// Always returns 401 — used by client-side logout to bust the browser's Basic Auth cache
static esp_err_t handleLogout(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"SailingComputer\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Logged out");
    return ESP_OK;
}

static esp_err_t handleRestart(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

static esp_err_t handleUM982Reset(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "[Web] UM982 factory reset requested");
    vTaskDelay(pdMS_TO_TICKS(100));
    if (uartNmeaMtx) xSemaphoreTake(uartNmeaMtx, portMAX_DELAY);
    um982FactoryReset(UART_NMEA);
    if (uartNmeaMtx) xSemaphoreGive(uartNmeaMtx);
    return ESP_OK;
}

static esp_err_t handleBleToggle(httpd_req_t *req) {
    char body[64] = ""; char val[16];
    int bodyLen = readBody(req, body, sizeof(body));
    if (bodyLen == -2) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_sendstr(req, "BLE form too large");
        return ESP_FAIL;
    }
    if (bodyLen < 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (formGet(body, "bleNmea", val, sizeof(val)))
        cfgMgr.cfg.bleNmea = (strcmp(val, "true") == 0 || strcmp(val, "on") == 0);
    cfgMgr.save();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

// ── OTA firmware update ────────────────────────────────────────────────────────
// Frontend sends raw binary (application/octet-stream), no multipart wrapper.

static const char OTA_SUCCESS_HTML[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA OK</title>"
    "<style>body{font-family:system-ui;background:#0a1628;color:#e0e8f0;"
    "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}"
    ".box{background:#0d1f3c;border:1px solid #1e3a5f;border-radius:8px;"
    "padding:2rem;max-width:400px;text-align:center;}"
    "h2{color:#4ade80;}.note{color:#8899aa;font-size:0.85rem;}</style>"
    "<script>setTimeout(()=>location.href='/',9000)</script></head>"
    "<body><div class='box'><h2>&#10003; Update Successful</h2>"
    "<p>Firmware flashed. Device is restarting&hellip;</p>"
    "<p class='note'>Redirecting in 9 seconds.</p>"
    "</div></body></html>";

static const char OTA_FAIL_HTML[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Failed</title>"
    "<style>body{font-family:system-ui;background:#0a1628;color:#e0e8f0;"
    "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}"
    ".box{background:#0d1f3c;border:1px solid #1e3a5f;border-radius:8px;"
    "padding:2rem;max-width:400px;text-align:center;}"
    "h2{color:#ff6b6b;} a{color:#4a9eff;}</style></head>"
    "<body><div class='box'><h2>&#10007; Update Failed</h2>"
    "<p>Flash error — check firmware.bin and retry.</p>"
    "<p><a href='/'>Back to dashboard</a></p>"
    "</div></body></html>";

static esp_err_t handleOTA(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    if (esp_ota_begin(update_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char    buf[1024];
    int     remaining = req->content_len;
    bool    ok        = true;

    while (remaining > 0 && ok) {
        int to_read = MIN(remaining, (int)sizeof(buf));
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) { ok = false; break; }
        if (esp_ota_write(ota_handle, buf, received) != ESP_OK) { ok = false; break; }
        remaining -= received;
    }

    if (ok) ok = (esp_ota_end(ota_handle) == ESP_OK);
    if (ok) ok = (esp_ota_set_boot_partition(update_part) == ESP_OK);

    if (ok) {
        ESP_LOGI(TAG, "[OTA] Flash complete — restarting");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, OTA_SUCCESS_HTML, HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else {
        esp_ota_abort(ota_handle);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, OTA_FAIL_HTML, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

// ── Basic Auth helper ─────────────────────────────────────────────────────────
// Returns true if the request carries valid admin credentials.
// Sends a 401 response and returns false otherwise.
static bool checkAuth(httpd_req_t *req) {
    char auth[160] = "";
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) == ESP_OK
        && strncmp(auth, "Basic ", 6) == 0)
    {
        unsigned char decoded[96] = {};
        size_t outlen = 0;
        if (mbedtls_base64_decode(decoded, sizeof(decoded)-1, &outlen,
                                  (const unsigned char*)(auth+6), strlen(auth+6)) == 0) {
            decoded[outlen] = '\0';
            char expected[96];
            snprintf(expected, sizeof(expected), "admin:%s", cfgMgr.cfg.adminPassword);
            if (strcmp((char*)decoded, expected) == 0) return true;
        }
    }
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"SailingComputer\"");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Unauthorized");
    return false;
}

// ── HTTP→HTTPS redirect handler ───────────────────────────────────────────────
static httpd_handle_t httpRedirectServer = NULL;

static esp_err_t handleRedirect(httpd_req_t *req) {
    char location[128];
    // Get host header for redirect target
    char host[64] = "";
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    // Strip port if present
    char *colon = strchr(host, ':');
    if (colon) *colon = '\0';
    if (strlen(host) == 0) strlcpy(host, "sailingcomputer.local", sizeof(host));
    snprintf(location, sizeof(location), "https://%s%s", host, req->uri);
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

// ── Web server setup ──────────────────────────────────────────────────────────

static void startWebServer() {
    // ── HTTPS server on port 443 ─────────────────────────────────────────────
    httpd_ssl_config_t ssl_cfg         = HTTPD_SSL_CONFIG_DEFAULT();
    ssl_cfg.httpd.max_open_sockets     = 2;  // 2 sessions × ~10KB each; lru_purge evicts idle ones
    ssl_cfg.httpd.max_uri_handlers     = 12; // default is 8; we register 9 handlers
    ssl_cfg.httpd.stack_size           = 10240;
    ssl_cfg.httpd.lru_purge_enable     = true;
    ssl_cfg.servercert                 = (const uint8_t *)server_cert_pem;
    ssl_cfg.servercert_len             = sizeof(server_cert_pem);
    ssl_cfg.prvtkey_pem                = (const uint8_t *)server_key_pem;
    ssl_cfg.prvtkey_len                = sizeof(server_key_pem);

    if (httpd_ssl_start(&webServer, &ssl_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "[Web] Failed to start HTTPS server");
        return;
    }

    auto reg = [](httpd_handle_t srv, const char *uri,
                  httpd_method_t method, esp_err_t (*fn)(httpd_req_t *)) {
        httpd_uri_t u = { .uri = uri, .method = method, .handler = fn, .user_ctx = NULL };
        httpd_register_uri_handler(srv, &u);
    };

    // Auth model: public = /, /status, /logout
    //             HTTP Basic Auth required = all others (admin:adminPassword)
    reg(webServer, "/",            HTTP_GET,  handleRoot);        // public — SPA HTML
    reg(webServer, "/status",      HTTP_GET,  handleStatus);      // public — live GNSS/NTRIP JSON
    reg(webServer, "/config",      HTTP_GET,  handleGetConfig);   // auth
    reg(webServer, "/config/save", HTTP_POST, handleSaveConfig);  // auth
    reg(webServer, "/restart",     HTTP_POST, handleRestart);     // auth
    reg(webServer, "/um982reset",  HTTP_POST, handleUM982Reset);  // auth
    reg(webServer, "/update",      HTTP_POST, handleOTA);         // auth — OTA firmware upload
    reg(webServer, "/ble/toggle",  HTTP_POST, handleBleToggle);   // auth
    reg(webServer, "/logout",      HTTP_GET,  handleLogout);      // public — always returns 401 to bust browser auth cache

    ESP_LOGI(TAG, "[Web] HTTPS server started on port 443");

    // ── HTTP redirect server on port 80 ──────────────────────────────────────
    httpd_config_t http_cfg    = HTTPD_DEFAULT_CONFIG();
    http_cfg.server_port       = 80;
    http_cfg.max_open_sockets  = 4;
    http_cfg.lru_purge_enable  = true;
    http_cfg.stack_size        = 4096;
    http_cfg.uri_match_fn      = httpd_uri_match_wildcard;

    if (httpd_start(&httpRedirectServer, &http_cfg) == ESP_OK) {
        httpd_uri_t redir = {
            .uri = "/*", .method = HTTP_GET,
            .handler = handleRedirect, .user_ctx = NULL
        };
        httpd_register_uri_handler(httpRedirectServer, &redir);
        ESP_LOGI(TAG, "[Web] HTTP→HTTPS redirect on port 80");
    }
}

// ── NMEA tasks ────────────────────────────────────────────────────────────────

static void nmeaRxTask(void *param) {
    uint8_t buf[128];
    NmeaMessage msg = {};
    size_t idx = 0;
    bool overflow = false;

    for (;;) {
        if (uartNmeaMtx && xSemaphoreTake(uartNmeaMtx, pdMS_TO_TICKS(20)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        int r = uart_read_bytes(UART_NMEA, buf, sizeof(buf), pdMS_TO_TICKS(10));
        if (uartNmeaMtx) xSemaphoreGive(uartNmeaMtx);
        if (r <= 0) continue;

        nmeaBytesRx += (uint32_t)r;
        for (int i = 0; i < r; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                if (overflow) {
                    nmeaOverflowDrops++;
                } else {
                    msg.line[idx] = '\0';
                    if (idx > 5) {
                        if (xQueueSend(nmeaParseQueue, &msg, 0) != pdTRUE)
                            nmeaParseDrops++;
                    }
                }
                idx = 0;
                overflow = false;
                msg = {};
            } else if (c != '\r') {
                if (!overflow && idx < sizeof(msg.line) - 1) {
                    msg.line[idx++] = c;
                } else {
                    overflow = true;
                }
            }
        }
    }
}

static void nmeaParseTask(void *param) {
    NmeaMessage msg;
    for (;;) {
        if (xQueueReceive(nmeaParseQueue, &msg, portMAX_DELAY) != pdTRUE) continue;

        bool emitHdt = false;
        bool hdtOk = false;
        float hdtHeading = 0.0f;

        if (telemetryMtx) xSemaphoreTake(telemetryMtx, portMAX_DELAY);
        nmeaLinesRx++;
        processNmeaLine(msg.line);
        if (strncmp(msg.line, "$GPHDT", 6) == 0 || strncmp(msg.line, "$GNHDT", 6) == 0) {
            emitHdt = true;
            hdtOk = hdtValid;
            hdtHeading = heading;
        }
        if (telemetryMtx) xSemaphoreGive(telemetryMtx);

        if (msg.line[0] == '$') {
            if (emitHdt && hdtOk) {
                char body[32], sentence[48];
                snprintf(body, sizeof(body), "GPHDT,%.4f,T", hdtHeading);
                snprintf(sentence, sizeof(sentence), "$%s*%02X", body, nmeaChecksum(body));
                enqueueNmeaOutput(sentence);
            } else if (!emitHdt) {
                enqueueNmeaOutput(msg.line);
            }
        }
    }
}

static void nmeaOutputTask(void *param) {
    NmeaMessage msg;
    for (;;) {
        if (nmeaSrvFd >= 0) acceptNmeaClients();
        if (xQueueReceive(nmeaOutputQueue, &msg, pdMS_TO_TICKS(20)) == pdTRUE) {
            broadcastNmea(msg.line);
            bleNmeaSend(msg.line);
        }
    }
}

// ── app_main ──────────────────────────────────────────────────────────────────

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "\n[Boot] SailingComputer starting...");

    // NVS must be initialised before any NVS operations or WiFi
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    cfgMgr.load();
    telemetryMtx = xSemaphoreCreateMutex();
    uartNmeaMtx  = xSemaphoreCreateMutex();
    ntripUpdateEnabled();

    // ── UART setup — exact sequence from Arduino ESP32 core ─────────────────
    // Order: param_config → set_pin → driver_install (matches Arduino internals).
    // No explicit gpio_set_direction — uart_set_pin handles it.
    // No loopback test — uart_set_loop_back bypasses GPIO16 and proved nothing.
    uart_config_t uc = {};
    uc.baud_rate           = NMEA_BAUD;
    uc.data_bits           = UART_DATA_8_BITS;
    uc.parity              = UART_PARITY_DISABLE;
    uc.stop_bits           = UART_STOP_BITS_1;
    uc.flow_ctrl           = UART_HW_FLOWCTRL_DISABLE;
    uc.rx_flow_ctrl_thresh = 122;             // Arduino default
    uc.source_clk          = UART_SCLK_DEFAULT;

    uart_param_config(UART_NMEA, &uc);
    uart_set_pin(UART_NMEA, SERIAL1_TX, SERIAL1_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NMEA, 2048, 0, 0, NULL, 0);

    uc.baud_rate = RTCM_BAUD;
    uart_param_config(UART_RTCM, &uc);
    uart_set_pin(UART_RTCM, SERIAL2_TX, SERIAL2_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_RTCM, 256, 1024, 0, NULL, 0);
    ESP_LOGI(TAG, "[Boot] UART initialized");

    um982Init(UART_NMEA);

    // ── WiFi ────────────────────────────────────────────────────────────────
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    Config &cfg = cfgMgr.cfg;
    if (cfg.apMode || strlen(cfg.wifiSSID) == 0) {
        cfg.apMode = true;
        startAP();
    } else {
        startSTA();
        // Wait up to WIFI_CONNECT_TIMEOUT_MS for connection
        uint32_t t0 = millis();
        while (!staConnected && (millis() - t0) < WIFI_CONNECT_TIMEOUT_MS)
            vTaskDelay(pdMS_TO_TICKS(200));
        if (!staConnected) {
            ESP_LOGW(TAG, "[WiFi] STA failed — falling back to AP");
            cfg.apMode = true;
            esp_wifi_stop();
            startAP();
        }
    }

    // ── mDNS ────────────────────────────────────────────────────────────────
    mdns_init();
    mdns_hostname_set("sailingcomputer");
    mdns_instance_name_set("SailingComputer");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "[mDNS] sailingcomputer.local");

    // ── NMEA TCP server ──────────────────────────────────────────────────────
    nmeaClientMtx = xSemaphoreCreateMutex();
    nmeaParseQueue  = xQueueCreate(32, sizeof(NmeaMessage));
    nmeaOutputQueue = xQueueCreate(32, sizeof(NmeaMessage));
    if (!telemetryMtx || !uartNmeaMtx || !nmeaClientMtx || !nmeaParseQueue || !nmeaOutputQueue) {
        ESP_LOGE(TAG, "[Boot] Failed to allocate synchronization primitives");
        return;
    }
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) nmeaClientFds[i] = -1;

    nmeaSrvFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(nmeaSrvFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in saddr = {};
    saddr.sin_family      = AF_INET;
    saddr.sin_port        = htons(NMEA_TCP_PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;
    bind(nmeaSrvFd,  (struct sockaddr *)&saddr, sizeof(saddr));
    listen(nmeaSrvFd, MAX_NMEA_CLIENTS);
    ESP_LOGI(TAG, "[NMEA] TCP server port %d", NMEA_TCP_PORT);

    // ── HTTP server ──────────────────────────────────────────────────────────
    startWebServer();

    // ── BLE ─────────────────────────────────────────────────────────────────
    if (cfg.bleNmea)
        bleNmeaInit("SailingComputer");

    // ── Tasks ────────────────────────────────────────────────────────────────
    // NTRIP on Core 0 (WiFi stack core) — DNS/TCP calls block here, not on Core 1
    xTaskCreatePinnedToCore(ntripTask, "ntrip", 8192, NULL, 2, NULL, 0);
    ESP_LOGI(TAG, "[NTRIP] Task started on Core 0");

    xTaskCreatePinnedToCore(nmeaRxTask, "nmea_rx", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(nmeaParseTask, "nmea_parse", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(nmeaOutputTask, "nmea_out", 4096, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "[NMEA] RX/parse/output tasks started");

    ESP_LOGI(TAG, "[Boot] Ready");
}
