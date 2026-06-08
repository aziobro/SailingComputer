#include <math.h>
#include <string.h>
#include <strings.h>
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
#include "version.h"
#include "webui.h"
#include "um982.h"
#include "ble_nmea.h"
#include "storage.h"
#include "gpx.h"
#include "track.h"
#include "cJSON.h"

#include "certs.h"
#include "mbedtls/base64.h"
#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_hosted.h"

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

// ── UART port assignments — ESP32-P4 ─────────────────────────────────────────
// UART_NUM_2 → UM982 COM2 (NMEA/control + RTCM): GPIO32=RX, GPIO33=TX @ 115200
// UART_NUM_1 → UM982 COM1 diagnostic link:       GPIO47=RX, GPIO48=TX @ 115200
// UART_NUM_0 reserved for USB/debug.
#define UART_NMEA  UART_NUM_2   // UM982 COM2 — NMEA/control
#define UART_RTCM  UART_NUM_1   // UM982 COM1 — auxiliary/probe

// ── State ─────────────────────────────────────────────────────────────────────

ConfigManager  cfgMgr;
StorageManager storageMgr;

struct NmeaMessage {
    char line[256];
};

static int   fixQuality = 0;
static int   satCount   = 0;
static float latitude   = 0, longitude = 0, heading = 0, sog = 0, cog = 0;
static float hdop = 0, altitude = 0;
static char  ggaUtcTime[12] = "000000.00"; // last UTC time from incoming GGA
static char  rmcDate[8]     = "010100";    // DDMMYY from last RMC sentence
static uint32_t gpsUnixTime = 0;           // UTC unix timestamp from RMC+GGA
static float roll = 0;          // heel: pitch of athwartships baseline
static float cogFiltered    = 0;
static bool  cogInitialized = false;
static bool  hdtValid       = false;
static bool  rollValid      = false;

static float leewayAngle        = 0;
static float lateralDrift       = 0;
static float driveSpeed         = 0;
static bool  sailingMetricsValid = false;

// ── Race state ────────────────────────────────────────────────────────────────

enum RaceState { RACE_IDLE, RACE_COUNTDOWN, RACE_RACING, RACE_COMPLETE };

struct StartLineEnd {
    char   markId[16];
    double lat, lon;
    char   name[32];
    bool   set;
};

struct RaceData {
    RaceState    state;
    int64_t      t0_ms;                         // millis since boot when T-0 fires
    int64_t      end_ms;                        // millis since boot when race ended
    int          duration_s;                    // sequence length in seconds
    StartLineEnd line[2];                       // [0]=port end, [1]=starboard end
    char         courseId[16];
    int          legIdx;
    int64_t      legTimes[MAX_COURSE_MARKS];    // time each mark was rounded (ms since boot)
    int          legTimesCount;                 // how many marks have been rounded
};

static RaceData raceData;

// ── Track recorder ────────────────────────────────────────────────────────────

static TrackRecorder trackRec;

// Diagnostic counters
static uint32_t nmeaBytesRx       = 0;  // raw bytes received from UART_NMEA
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
static uint32_t ntripLastGgaTx   = 0;  // global — persists across reconnects
static uint32_t ntripBytesIn     = 0;
static uint32_t ntripConnectTime = 0;
static uint32_t ntripSessionBytes = 0;
static uint32_t ntripBytesToUart = 0;
static uint32_t rtcmFramesIn     = 0;
static uint16_t rtcmLastType     = 0;
static bool     rtcmUartProbeOk  = false;
static int      ntripActiveIdx   = 0;
static int      ntripFailCount   = 0;
static bool     ntripAnyEnabled  = false;

// RTCM3 framing: D3, 10-bit payload length, payload, then CRC24Q.
static uint16_t rtcmPayloadRemaining = 0;
static uint8_t  rtcmHeader[3] = {};
static uint8_t  rtcmHeaderPos = 0;
static uint8_t  rtcmCrcPos = 0;
static uint32_t rtcmCrcCalc = 0;
static uint32_t rtcmCrcReceived = 0;
static uint16_t rtcmCurrentType = 0;
static uint8_t  rtcmPayloadPos = 0;

static uint32_t rtcmCrc24qByte(uint32_t crc, uint8_t data) {
    crc ^= (uint32_t)data << 16;
    for (int bit = 0; bit < 8; bit++) {
        crc <<= 1;
        if (crc & 0x1000000) crc ^= 0x1864CFB;
    }
    return crc & 0xFFFFFF;
}

static void countRtcmFrames(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (rtcmPayloadRemaining > 0) {
            if (rtcmPayloadPos == 0) {
                rtcmCurrentType = (uint16_t)b << 4;
            } else if (rtcmPayloadPos == 1) {
                rtcmCurrentType |= b >> 4;
            }
            rtcmPayloadPos++;
            rtcmCrcCalc = rtcmCrc24qByte(rtcmCrcCalc, b);
            if (--rtcmPayloadRemaining == 0) rtcmCrcPos = 1;
            continue;
        }
        if (rtcmCrcPos > 0) {
            rtcmCrcReceived = (rtcmCrcReceived << 8) | b;
            if (++rtcmCrcPos == 4) {
                if (rtcmCrcReceived == rtcmCrcCalc) {
                    rtcmFramesIn++;
                    rtcmLastType = rtcmCurrentType;
                }
                rtcmCrcPos = 0;
            }
            continue;
        }
        if (rtcmHeaderPos == 0) {
            if (b == 0xD3) {
                rtcmHeader[0] = b;
                rtcmHeaderPos = 1;
                rtcmCrcCalc = rtcmCrc24qByte(0, b);
            }
            continue;
        }
        rtcmHeader[rtcmHeaderPos++] = b;
        rtcmCrcCalc = rtcmCrc24qByte(rtcmCrcCalc, b);
        if (rtcmHeaderPos == 3) {
            rtcmPayloadRemaining =
                ((uint16_t)(rtcmHeader[1] & 0x03) << 8) | rtcmHeader[2];
            rtcmHeaderPos = 0;
            rtcmCrcReceived = 0;
            rtcmCurrentType = 0;
            rtcmPayloadPos = 0;
            rtcmCrcPos = rtcmPayloadRemaining == 0 ? 1 : 0;
        }
    }
}

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

// Split comma-separated NMEA fields in place while preserving empty fields.
// strtok() cannot be used here because it collapses empty position fields
// while the receiver has no fix.
static int splitNmeaFields(char *line, char **fields, int maxFields) {
    int count = 0;
    char *p = line;
    while (count < maxFields) {
        fields[count++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    if (count > 0) {
        char *star = strchr(fields[count - 1], '*');
        if (star) *star = '\0';
    }
    return count;
}

static void parseGGA(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *f[16] = {};
    int count = splitNmeaFields(buf, f, 16);
    if (count < 10) return;

    fixQuality = atoi(f[6]);
    satCount   = atoi(f[7]);
    hdop       = atof(f[8]);
    altitude   = atof(f[9]);
    if (f[1] && f[1][0]) strlcpy(ggaUtcTime, f[1], sizeof(ggaUtcTime));

    if (f[2][0] && f[4][0]) {
        latitude  = nmeaToDeg(atof(f[2]));
        longitude = nmeaToDeg(atof(f[4]));
        if (f[3][0] == 'S') latitude  = -latitude;
        if (f[5][0] == 'W') longitude = -longitude;
    }

    trackRec.tryWrite(latitude, longitude, heading, roll,
                      sog, cog, (uint8_t)fixQuality, gpsUnixTime);
}

// Parse $GNRMC / $GPRMC to extract date (field 9: DDMMYY) and recompute
// gpsUnixTime combining the date with the UTC time already in ggaUtcTime.
static void parseRMC(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *f[12] = {};
    if (splitNmeaFields(buf, f, 12) < 10) return;
    if (!f[9] || strlen(f[9]) < 6) return;
    strlcpy(rmcDate, f[9], sizeof(rmcDate));

    // Recompute unix timestamp from rmcDate (DDMMYY) + ggaUtcTime (HHMMSS.ss)
    const char *d = rmcDate;
    const char *t = ggaUtcTime;
    if (strlen(d) < 6 || strlen(t) < 6) return;
    struct tm tm_ = {};
    tm_.tm_mday  = (d[0]-'0')*10 + (d[1]-'0');
    tm_.tm_mon   = (d[2]-'0')*10 + (d[3]-'0') - 1;
    int yy       = (d[4]-'0')*10 + (d[5]-'0');
    tm_.tm_year  = (yy + 2000) - 1900;
    tm_.tm_hour  = (t[0]-'0')*10 + (t[1]-'0');
    tm_.tm_min   = (t[2]-'0')*10 + (t[3]-'0');
    tm_.tm_sec   = (t[4]-'0')*10 + (t[5]-'0');
    tm_.tm_isdst = 0;
    time_t ts = mktime(&tm_);  // TZ must be set to UTC0 at startup
    if (ts > 0) gpsUnixTime = (uint32_t)ts;
}

static void parseHDT(const char *s) {
    char buf[256]; strlcpy(buf, s, sizeof(buf));
    char *f[4] = {};
    int count = splitNmeaFields(buf, f, 4);
    if (count > 1 && f[1][0]) {
        heading  = applyHeadingOffset(atof(f[1]));
        hdtValid = true;
        updateSailingMetrics();
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
    char *f[12] = {};
    int count = splitNmeaFields(buf, f, 12);
    float rawCog = (count > 1 && f[1][0]) ? atof(f[1]) : cog;
    if (count > 5 && f[5][0]) sog = atof(f[5]);
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
    else if (strncmp(line, "$GNRMC",    6) == 0 || strncmp(line, "$GPRMC",    6) == 0) parseRMC(line);
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

// Accept a bare host as intended, but also clean up common URL-style entries
// such as "http://192.168.8.195/" or "192.168.8.195:2101/MOUNT".
static bool normalizeNtripSource(NtripSource &src) {
    char originalHost[sizeof(src.host)];
    char originalMount[sizeof(src.mount)];
    strlcpy(originalHost, src.host, sizeof(originalHost));
    strlcpy(originalMount, src.mount, sizeof(originalMount));

    char value[sizeof(src.host)];
    strlcpy(value, src.host, sizeof(value));

    char *start = value;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '/'))
        *--end = '\0';

    if (strncasecmp(start, "http://", 7) == 0) {
        start += 7;
    } else if (strncasecmp(start, "https://", 8) == 0) {
        start += 8;
        ESP_LOGW(TAG, "[NTRIP] HTTPS URL supplied; NTRIP socket uses plain TCP");
    }

    char *path = strchr(start, '/');
    if (path) {
        *path++ = '\0';
        if (src.mount[0] == '\0' && *path)
            strlcpy(src.mount, path, sizeof(src.mount));
    }

    // Handle the common host:port form. Bracketed IPv6 is left untouched.
    char *colon = strrchr(start, ':');
    if (colon && strchr(start, ':') == colon) {
        char *portText = colon + 1;
        char *p = portText;
        while (*p >= '0' && *p <= '9') p++;
        if (*portText && *p == '\0') {
            int parsedPort = atoi(portText);
            if (parsedPort >= 1 && parsedPort <= 65535) {
                src.port = (uint16_t)parsedPort;
                *colon = '\0';
            }
        }
    }

    while (src.mount[0] == '/')
        memmove(src.mount, src.mount + 1, strlen(src.mount));
    strlcpy(src.host, start, sizeof(src.host));

    bool changed = strcmp(originalHost, src.host) != 0 ||
                   strcmp(originalMount, src.mount) != 0;
    if (changed) {
        ESP_LOGI(TAG, "[NTRIP] Normalized source to %s:%u/%s",
                 src.host, src.port, src.mount);
    }
    return changed;
}

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
    ntripLastAttempt = millis();
}

// Complete a TCP write even if lwIP accepts only part of the buffer. A partial
// HTTP request can make the caster interpret later GGA sentences as a malformed
// connection request.
static bool socketSendAll(int sock, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int r = send(sock, data + sent, len - sent, 0);
        if (r > 0) {
            sent += (size_t)r;
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        ESP_LOGE(TAG, "[NTRIP] TCP send failed after %u/%u bytes: errno %d",
                 (unsigned)sent, (unsigned)len, errno);
        return false;
    }
    return true;
}

// Send one GGA to the NTRIP caster. Ten seconds is sufficient for moving-base
// and VRS services without producing unnecessary client traffic.
#define NTRIP_GGA_INTERVAL_MS 10000

static void ntripSendGGA(bool force = false) {
    if (ntripSock < 0) return;

    // Rate-limit: drop the call if we sent too recently
    uint32_t now = millis();
    if (!force && now - ntripLastGgaTx < NTRIP_GGA_INTERVAL_MS) return;

    // Snapshot telemetry
    int q, sats;
    float latRaw, lonRaw, hdopCopy, altCopy;
    char utc[12];
    if (telemetryMtx) xSemaphoreTake(telemetryMtx, portMAX_DELAY);
    q        = fixQuality;
    sats     = satCount;
    latRaw   = latitude;
    lonRaw   = longitude;
    hdopCopy = hdop;
    altCopy  = altitude;
    strlcpy(utc, ggaUtcTime, sizeof(utc));
    if (telemetryMtx) xSemaphoreGive(telemetryMtx);

    // Validate: need a real fix and plausible non-zero coordinates
    if (q == 0) { ESP_LOGD(TAG, "[NTRIP] GGA skipped — no fix"); return; }
    if (latRaw == 0.0f && lonRaw == 0.0f) { ESP_LOGW(TAG, "[NTRIP] GGA skipped — zero position"); return; }
    if (fabsf(latRaw) > 90.0f || fabsf(lonRaw) > 180.0f) { ESP_LOGW(TAG, "[NTRIP] GGA skipped — out-of-range coords"); return; }

    // Build sentence
    float lat    = fabsf(latRaw), lon = fabsf(lonRaw);
    int   latDeg = (int)lat;  float latMin = (lat - latDeg) * 60.0f;
    int   lonDeg = (int)lon;  float lonMin = (lon - lonDeg) * 60.0f;
    char body[100];
    snprintf(body, sizeof(body),
             "GPGGA,%s,%02d%08.5f,%c,%03d%08.5f,%c,%d,%02d,%.1f,%.1f,M,0.0,M,,",
             utc,
             latDeg, latMin, latRaw >= 0 ? 'N' : 'S',
             lonDeg, lonMin, lonRaw >= 0 ? 'E' : 'W',
             q, sats, hdopCopy, altCopy);
    char sentence[120];
    snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, nmeaChecksum(body));

    if (!socketSendAll(ntripSock, sentence, strlen(sentence))) {
        ESP_LOGE(TAG, "[NTRIP%d] GGA send failed", ntripActiveIdx);
        ntripDisconnect();
        ntripFailCount++;
        return;
    }
    ntripLastGgaTx = millis();
    ESP_LOGD(TAG, "[NTRIP%d] Sent GGA: %s", ntripActiveIdx, sentence);
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
    normalizeNtripSource(src);
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
    if (!socketSendAll(sock, req, strlen(req))) {
        ESP_LOGE(TAG, "[NTRIP%d] Request send failed", idx);
        close(sock); return false;
    }

    // Validate the status line exactly. Searching every header for "200" can
    // turn an error response (for example Content-Length: 200) into a false
    // successful connection, after which periodic GGA looks like a garbled
    // request to the caster.
    char line[256];
    int statusLen = sockReadLine(sock, line, sizeof(line));
    if (statusLen < 0) {
        ESP_LOGE(TAG, "[NTRIP%d] No response status", idx);
        close(sock); return false;
    }
    ESP_LOGI(TAG, "[NTRIP%d] < %s", idx, line);
    bool http200 = strncmp(line, "HTTP/1.0 200 ", 13) == 0 ||
                   strncmp(line, "HTTP/1.1 200 ", 13) == 0;
    bool icy200  = strcmp(line, "ICY 200 OK") == 0;
    if (!http200 && !icy200) {
        ESP_LOGE(TAG, "[NTRIP%d] Rejected status: %s", idx, line);
        close(sock); return false;
    }

    // HTTP responses must end with a blank line. ICY/NTRIP v1 streams may put
    // RTCM immediately after the single status line, so leave those bytes alone.
    bool headersComplete = icy200;
    bool nonStreamContent = false;
    for (int tries = 0; http200 && tries < 30; tries++) {
        int n = sockReadLine(sock, line, sizeof(line));
        if (n < 0) break;
        ESP_LOGI(TAG, "[NTRIP%d] < %s", idx, line);
        if (n == 0) {
            headersComplete = true;
            break;
        }
        if (strncasecmp(line, "Content-Type:", 13) == 0 &&
            (strstr(line, "text/") || strstr(line, "html") ||
             strstr(line, "json") || strstr(line, "xml"))) {
            nonStreamContent = true;
        }
    }
    if (!headersComplete) {
        ESP_LOGE(TAG, "[NTRIP%d] Incomplete response headers", idx);
        close(sock); return false;
    }
    if (nonStreamContent) {
        ESP_LOGE(TAG, "[NTRIP%d] Response is not a correction stream", idx);
        close(sock); return false;
    }

    ntripSock         = sock;
    ntripConnected    = true;
    ntripFailCount    = 0;
    ntripConnectTime  = millis();
    ntripSessionBytes = 0;
    ESP_LOGI(TAG, "[NTRIP%d] Connected OK", idx);
    ntripSendGGA(true);
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
                ntripFailCount++;
                return;
            } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "[NTRIP%d] Socket error %d", ntripActiveIdx, errno);
                ntripDisconnect();
                ntripFailCount++;
                return;
            }
        }
    }

    // Send GGA periodically — ntripSendGGA() enforces NTRIP_GGA_INTERVAL_MS internally
    ntripSendGGA();

    // Inject corrections on COM2, whose receiver input is already proven by
    // the successful startup configuration commands. NMEA output is on the
    // opposite wire of this full-duplex UART and is unaffected.
    uint8_t buf[256];
    int n = recv(ntripSock, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0) {
        countRtcmFrames(buf, n);
        if (uartNmeaMtx) xSemaphoreTake(uartNmeaMtx, portMAX_DELAY);
        int written = uart_write_bytes(UART_NMEA, (const char *)buf, n);
        if (uartNmeaMtx) xSemaphoreGive(uartNmeaMtx);
        if (written < n)
            ESP_LOGW(TAG, "[NTRIP%d] RTCM UART short write %d/%d", ntripActiveIdx, written, n);
        if (written > 0) ntripBytesToUart += written;
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

// ESP32-P4 hosted WiFi can't read MAC from C6 coprocessor in ESP-IDF 5.3.2,
// so derive a locally-administered MAC from the chip's own eFuse ID instead.
static void setWifiMacFromEfuse(wifi_interface_t iface) {
    uint8_t base[6] = {};
    esp_efuse_mac_get_default(base);
    // Bit 1 of first byte = locally administered, bit 0 = unicast
    base[0] = (base[0] & 0xFE) | 0x02;
    if (iface == WIFI_IF_AP) base[5] ^= 0x01;  // differentiate AP from STA
    esp_err_t err = esp_wifi_set_mac(iface, base);
    ESP_LOGI(TAG, "[WiFi] MAC set %02x:%02x:%02x:%02x:%02x:%02x (%s)",
             base[0], base[1], base[2], base[3], base[4], base[5],
             err == ESP_OK ? "ok" : esp_err_to_name(err));
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
    setWifiMacFromEfuse(WIFI_IF_AP);
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
    setWifiMacFromEfuse(WIFI_IF_STA);
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
        "\"ntripBytesToUart\":%u,"
        "\"rtcmFramesIn\":%u,"
        "\"rtcmLastType\":%u,"
        "\"rtcmUartProbeOk\":%s,"
        "\"nmeaBytesRx\":%u,"
        "\"nmeaLinesRx\":%u,"
        "\"nmeaOverflowDrops\":%u,"
        "\"nmeaParseDrops\":%u,"
        "\"nmeaOutputDrops\":%u,"
        "\"nmeaTcpDrops\":%u,"
        "\"bleDrops\":%u,"
        "\"wifiMode\":\"%s\","
        "\"ip\":\"%s\","
        "\"apIP\":\"%s\","
        "\"version\":\"%s\""
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
        ntripActiveIdx+1, ntripBytesIn, ntripBytesToUart,
        rtcmFramesIn, rtcmLastType,
        rtcmUartProbeOk ? "true" : "false",
        nmeaBytesRx, nmeaLinesRx,
        nmeaOverflowDrops, nmeaParseDrops, nmeaOutputDrops, nmeaTcpDrops,
        bleDropCount,
        cfgMgr.cfg.apMode ? "AP" : "Station",
        staIP, apIP, FW_VERSION
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
        "],\"headingOffset\":%.1f,\"cogMinSog\":%.2f,\"bleNmea\":%s,\"gpsUpdateRate\":%d,\"apSSID\":",
        cfg.headingOffset, cfg.cogMinSog,
        cfg.bleNmea ? "true" : "false",
        cfg.gpsUpdateRate);
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

        normalizeNtripSource(cfg.ntrip[i]);
    }

    if (formGet(body, "headingOffset",  val, sizeof(val))) cfg.headingOffset = atof(val);
    if (formGet(body, "cogMinSog",      val, sizeof(val))) cfg.cogMinSog     = atof(val);
    if (formGet(body, "gpsUpdateRate",  val, sizeof(val))) {
        const uint8_t validRates[] = {1, 2, 5, 10, 20};
        uint8_t r = (uint8_t)atoi(val);
        cfg.gpsUpdateRate = 1;
        for (uint8_t i = 0; i < sizeof(validRates); i++)
            if (r == validRates[i]) { cfg.gpsUpdateRate = r; break; }
    }
    if (formGet(body, "bleNmea",        val, sizeof(val)))
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
    um982FactoryReset(UART_NMEA, cfgMgr.cfg.gpsUpdateRate);
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

// ── Racing / storage HTTP handlers ───────────────────────────────────────────

static esp_err_t handleGetMarks(httpd_req_t *req) {
    Mark marks[MAX_MARKS];
    int count = storageMgr.loadMarks(marks, MAX_MARKS);
    char *json = storageMgr.marksToJson(marks, count);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json ? json : "[]", HTTPD_RESP_USE_STRLEN);
    if (json) free(json);
    return ESP_OK;
}

static esp_err_t handlePostMark(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[512];
    if (readBody(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_OK;
    }
    cJSON *obj = cJSON_Parse(body);
    if (!obj) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }
    Mark m = {};
    storageMgr.generateMarkId(m.id, sizeof(m.id));
    cJSON *v;
    if ((v = cJSON_GetObjectItem(obj, "name")) && cJSON_IsString(v)) strlcpy(m.name, v->valuestring, sizeof(m.name));
    if ((v = cJSON_GetObjectItem(obj, "lat"))  && cJSON_IsNumber(v)) m.lat = v->valuedouble;
    if ((v = cJSON_GetObjectItem(obj, "lon"))  && cJSON_IsNumber(v)) m.lon = v->valuedouble;
    m.created = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    cJSON_Delete(obj);

    if (strlen(m.name) == 0) strlcpy(m.name, "Mark", sizeof(m.name));

    char resp[64];
    if (storageMgr.saveMark(m)) {
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":\"%s\"}", m.id);
    } else {
        strlcpy(resp, "{\"ok\":false}", sizeof(resp));
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleDeleteMark(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[64];
    readBody(req, body, sizeof(body));
    cJSON *obj = cJSON_Parse(body);
    const char *id = nullptr;
    cJSON *v;
    if (obj && (v = cJSON_GetObjectItem(obj, "id")) && cJSON_IsString(v)) id = v->valuestring;
    bool ok = id && storageMgr.deleteMark(id);
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok ? "{\"ok\":true}" : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleGetCourses(httpd_req_t *req) {
    Course *courses = (Course *)malloc(MAX_COURSES * sizeof(Course));
    if (!courses) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); return ESP_OK; }
    int count = storageMgr.loadCourses(courses, MAX_COURSES);
    char *json = storageMgr.coursesToJson(courses, count);
    free(courses);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json ? json : "[]", HTTPD_RESP_USE_STRLEN);
    if (json) free(json);
    return ESP_OK;
}

static esp_err_t handleStorageInfo(httpd_req_t *req) {
    uint64_t total = 0, used = 0;
    bool available = storageMgr.getInfo(&total, &used);
    uint64_t freeBytes = used <= total ? total - used : 0;
    char json[256];
    snprintf(json, sizeof(json),
             "{\"available\":%s,\"backend\":\"%s\",\"mount_point\":\"%s\","
             "\"total\":%llu,\"used\":%llu,\"free\":%llu}",
             available ? "true" : "false", storageMgr.backendName(),
             storageMgr.mountPoint(),
             (unsigned long long)total, (unsigned long long)used,
             (unsigned long long)freeBytes);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ── Race sequence HTTP handlers ───────────────────────────────────────────────

static esp_err_t handleRaceState(httpd_req_t *req) {
    int64_t now = (int64_t)(esp_timer_get_time() / 1000LL);

    // Auto-transition COUNTDOWN → RACING when T-0 passes
    if (raceData.state == RACE_COUNTDOWN && now >= raceData.t0_ms)
        raceData.state = RACE_RACING;

    const char *stateStr =
        raceData.state == RACE_COUNTDOWN ? "countdown" :
        raceData.state == RACE_RACING    ? "racing"    :
        raceData.state == RACE_COMPLETE  ? "complete"  : "idle";

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "state",         stateStr);
    cJSON_AddNumberToObject(obj, "t0_ms",         (double)raceData.t0_ms);
    cJSON_AddNumberToObject(obj, "end_ms",        (double)raceData.end_ms);
    cJSON_AddNumberToObject(obj, "server_now_ms", (double)now);
    cJSON_AddNumberToObject(obj, "duration_s",    raceData.duration_s);
    cJSON_AddStringToObject(obj, "courseId",      raceData.courseId);
    cJSON_AddNumberToObject(obj, "legIdx",        raceData.legIdx);

    cJSON *lineArr = cJSON_AddArrayToObject(obj, "line");
    for (int i = 0; i < 2; i++) {
        cJSON *end = cJSON_CreateObject();
        cJSON_AddBoolToObject(end,   "set",    raceData.line[i].set);
        cJSON_AddStringToObject(end, "markId", raceData.line[i].markId);
        cJSON_AddStringToObject(end, "name",   raceData.line[i].name);
        cJSON_AddNumberToObject(end, "lat",    raceData.line[i].lat);
        cJSON_AddNumberToObject(end, "lon",    raceData.line[i].lon);
        cJSON_AddItemToArray(lineArr, end);
    }

    // Look up course data for next mark (racing) or leg stats (complete)
    if (raceData.courseId[0] && raceData.state != RACE_IDLE) {
        Course *courses = (Course *)malloc(MAX_COURSES * sizeof(Course));
        if (courses) {
            int nc = storageMgr.loadCourses(courses, MAX_COURSES);
            for (int i = 0; i < nc; i++) {
                if (strcmp(courses[i].id, raceData.courseId) == 0) {
                    cJSON_AddStringToObject(obj, "courseName",      courses[i].name);
                    cJSON_AddNumberToObject(obj, "courseTotalMarks", courses[i].mark_count);

                    if (raceData.state == RACE_RACING && raceData.legIdx < courses[i].mark_count) {
                        // Next mark for live navigation
                        const char *markId = courses[i].marks[raceData.legIdx].mark_id;
                        Mark marks[MAX_MARKS];
                        int nm = storageMgr.loadMarks(marks, MAX_MARKS);
                        for (int j = 0; j < nm; j++) {
                            if (strcmp(marks[j].id, markId) == 0) {
                                cJSON *nm_obj = cJSON_CreateObject();
                                cJSON_AddStringToObject(nm_obj, "name", marks[j].name);
                                cJSON_AddNumberToObject(nm_obj, "lat",  marks[j].lat);
                                cJSON_AddNumberToObject(nm_obj, "lon",  marks[j].lon);
                                cJSON_AddItemToObject(obj, "nextMark", nm_obj);
                                break;
                            }
                        }
                    }

                    if (raceData.state == RACE_COMPLETE && raceData.legTimesCount > 0) {
                        // Build leg splits for stats page
                        Mark marks[MAX_MARKS];
                        int nm = storageMgr.loadMarks(marks, MAX_MARKS);
                        cJSON *legsArr = cJSON_AddArrayToObject(obj, "legs");
                        int64_t prev = raceData.t0_ms;
                        for (int k = 0; k < raceData.legTimesCount && k < courses[i].mark_count; k++) {
                            const char *mid = courses[i].marks[k].mark_id;
                            const char *mname = mid;
                            for (int j = 0; j < nm; j++) {
                                if (strcmp(marks[j].id, mid) == 0) { mname = marks[j].name; break; }
                            }
                            int64_t t   = raceData.legTimes[k];
                            cJSON *leg  = cJSON_CreateObject();
                            cJSON_AddStringToObject(leg, "mark",      mname);
                            cJSON_AddNumberToObject(leg, "elapsed_s", (double)((t - raceData.t0_ms) / 1000LL));
                            cJSON_AddNumberToObject(leg, "split_s",   (double)((t - prev) / 1000LL));
                            cJSON_AddItemToArray(legsArr, leg);
                            prev = t;
                        }
                    }
                    break;
                }
            }
            free(courses);
        }
    }

    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json ? json : "{}", HTTPD_RESP_USE_STRLEN);
    if (json) free(json);
    return ESP_OK;
}

static esp_err_t handleRaceStart(httpd_req_t *req) {
    if (raceData.state != RACE_IDLE) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not idle\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    int64_t now    = (int64_t)(esp_timer_get_time() / 1000LL);
    raceData.t0_ms = now + (int64_t)raceData.duration_s * 1000LL;
    raceData.state = RACE_COUNTDOWN;
    raceData.legIdx = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceStop(httpd_req_t *req) {
    memset(&raceData, 0, sizeof(raceData));
    raceData.duration_s = 300;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceEnd(httpd_req_t *req) {
    if (raceData.state != RACE_RACING) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not racing\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    raceData.state  = RACE_COMPLETE;
    raceData.end_ms = (int64_t)(esp_timer_get_time() / 1000LL);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceSync(httpd_req_t *req) {
    if (raceData.state == RACE_IDLE || raceData.state == RACE_COMPLETE) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not active\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    int64_t now       = (int64_t)(esp_timer_get_time() / 1000LL);
    int64_t remaining = raceData.t0_ms - now;
    // Round remaining to nearest whole minute
    int64_t snapped   = ((remaining + 30000LL) / 60000LL) * 60000LL;
    raceData.t0_ms    = now + snapped;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceDuration(httpd_req_t *req) {
    if (raceData.state != RACE_IDLE) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not idle\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    char body[64];
    readBody(req, body, sizeof(body));
    cJSON *obj = cJSON_Parse(body);
    cJSON *v;
    if (obj && (v = cJSON_GetObjectItem(obj, "seconds")) && cJSON_IsNumber(v)) {
        int sec = (int)v->valuedouble;
        if (sec >= 60 && sec <= 1800) raceData.duration_s = sec;
    }
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceStartLine(httpd_req_t *req) {
    char body[256];
    if (readBody(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_OK;
    }
    cJSON *obj = cJSON_Parse(body);
    if (!obj) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }
    cJSON *v;
    int endIdx = -1;
    if ((v = cJSON_GetObjectItem(obj, "end")) && cJSON_IsNumber(v)) endIdx = (int)v->valuedouble;
    if (endIdx < 0 || endIdx > 1) {
        cJSON_Delete(obj);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "end must be 0 or 1");
        return ESP_OK;
    }

    StartLineEnd &lineEnd = raceData.line[endIdx];
    memset(&lineEnd, 0, sizeof(lineEnd));

    cJSON *markIdJ = cJSON_GetObjectItem(obj, "markId");
    if (markIdJ && cJSON_IsString(markIdJ) && markIdJ->valuestring[0]) {
        strlcpy(lineEnd.markId, markIdJ->valuestring, sizeof(lineEnd.markId));
        Mark marks[MAX_MARKS];
        int nm = storageMgr.loadMarks(marks, MAX_MARKS);
        for (int i = 0; i < nm; i++) {
            if (strcmp(marks[i].id, lineEnd.markId) == 0) {
                lineEnd.lat = marks[i].lat;
                lineEnd.lon = marks[i].lon;
                strlcpy(lineEnd.name, marks[i].name, sizeof(lineEnd.name));
                lineEnd.set = true;
                break;
            }
        }
    } else {
        if ((v = cJSON_GetObjectItem(obj, "lat"))  && cJSON_IsNumber(v)) lineEnd.lat = v->valuedouble;
        if ((v = cJSON_GetObjectItem(obj, "lon"))  && cJSON_IsNumber(v)) lineEnd.lon = v->valuedouble;
        if ((v = cJSON_GetObjectItem(obj, "name")) && cJSON_IsString(v)) strlcpy(lineEnd.name, v->valuestring, sizeof(lineEnd.name));
        if (lineEnd.name[0] == '\0') strlcpy(lineEnd.name, endIdx == 0 ? "Port End" : "Stbd End", sizeof(lineEnd.name));
        lineEnd.set = (lineEnd.lat != 0.0 || lineEnd.lon != 0.0);
    }
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, lineEnd.set ? "{\"ok\":true}" : "{\"ok\":false,\"err\":\"mark not found\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceCourse(httpd_req_t *req) {
    char body[64];
    readBody(req, body, sizeof(body));
    cJSON *obj = cJSON_Parse(body);
    cJSON *v;
    if (obj) {
        if ((v = cJSON_GetObjectItem(obj, "courseId")) && cJSON_IsString(v))
            strlcpy(raceData.courseId, v->valuestring, sizeof(raceData.courseId));
        if ((v = cJSON_GetObjectItem(obj, "leg")) && cJSON_IsNumber(v))
            raceData.legIdx = (int)v->valuedouble;
        cJSON_Delete(obj);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRaceNextLeg(httpd_req_t *req) {
    if (raceData.state != RACE_RACING) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not racing\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    int64_t now = (int64_t)(esp_timer_get_time() / 1000LL);
    // Record the time this mark was rounded
    if (raceData.legIdx < MAX_COURSE_MARKS)
        raceData.legTimes[raceData.legTimesCount++] = now;

    bool complete = false;
    if (raceData.courseId[0]) {
        Course *courses = (Course *)malloc(MAX_COURSES * sizeof(Course));
        if (courses) {
            int nc = storageMgr.loadCourses(courses, MAX_COURSES);
            for (int i = 0; i < nc; i++) {
                if (strcmp(courses[i].id, raceData.courseId) == 0) {
                    raceData.legIdx++;
                    if (raceData.legIdx >= courses[i].mark_count) {
                        raceData.state  = RACE_COMPLETE;
                        raceData.end_ms = now;
                        complete = true;
                    }
                    break;
                }
            }
            free(courses);
        }
    } else {
        raceData.legIdx++;
    }
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"complete\":%s}", complete ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleRacePrevLeg(httpd_req_t *req) {
    if (raceData.state != RACE_RACING) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"err\":\"not racing\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    if (raceData.legIdx > 0) raceData.legIdx--;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ── File manager HTTP handlers ────────────────────────────────────────────────

// Read the "path" query parameter and percent-decode it into dst.
static void queryPath(httpd_req_t *req, char *dst, size_t dstSize) {
    dst[0] = '\0';
    size_t qLen = httpd_req_get_url_query_len(req);
    if (qLen == 0 || qLen >= dstSize + 64) return;
    char qbuf[256];
    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) != ESP_OK) return;
    char raw[128] = {};
    if (httpd_query_key_value(qbuf, "path", raw, sizeof(raw)) != ESP_OK) return;
    urlDecode(dst, raw, dstSize);
}

static esp_err_t handleFilesList(httpd_req_t *req) {
    char path[128] = {};
    queryPath(req, path, sizeof(path));
    if (path[0] == '\0') strlcpy(path, storageMgr.mountPoint(), sizeof(path));
    const char *safe = storageMgr.safePath(path);
    if (!safe) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path"); return ESP_OK; }
    char *json = storageMgr.listDir(safe);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json ? json : "[]", HTTPD_RESP_USE_STRLEN);
    if (json) free(json);
    return ESP_OK;
}

static esp_err_t handleFilesRename(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[512];
    if (readBody(req, body, sizeof(body)) < 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large"); return ESP_OK; }
    cJSON *obj = cJSON_Parse(body);
    const char *from = nullptr, *to = nullptr;
    cJSON *v;
    if (obj) {
        if ((v = cJSON_GetObjectItem(obj, "from")) && cJSON_IsString(v)) from = v->valuestring;
        if ((v = cJSON_GetObjectItem(obj, "to"))   && cJSON_IsString(v)) to   = v->valuestring;
    }
    bool ok = from && to &&
              storageMgr.safePath(from) && storageMgr.safePath(to) &&
              storageMgr.renameEntry(from, to);
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok ? "{\"ok\":true}" : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleFilesDelete(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[256];
    readBody(req, body, sizeof(body));
    cJSON *obj = cJSON_Parse(body);
    const char *path = nullptr;
    cJSON *v;
    if (obj && (v = cJSON_GetObjectItem(obj, "path")) && cJSON_IsString(v)) path = v->valuestring;
    bool ok = path && storageMgr.safePath(path) && storageMgr.deleteEntry(path);
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok ? "{\"ok\":true}" : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleFilesCopy(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    char body[512];
    readBody(req, body, sizeof(body));
    cJSON *obj = cJSON_Parse(body);
    const char *src = nullptr, *dst = nullptr;
    cJSON *v;
    if (obj) {
        if ((v = cJSON_GetObjectItem(obj, "src")) && cJSON_IsString(v)) src = v->valuestring;
        if ((v = cJSON_GetObjectItem(obj, "dst")) && cJSON_IsString(v)) dst = v->valuestring;
    }
    bool ok = src && dst &&
              storageMgr.safePath(src) && storageMgr.safePath(dst) &&
              storageMgr.copyFile(src, dst);
    cJSON_Delete(obj);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok ? "{\"ok\":true}" : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleFilesDownload(httpd_req_t *req) {
    char path[128] = {};
    queryPath(req, path, sizeof(path));
    const char *safe = storageMgr.safePath(path);
    if (!safe || !path[0]) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path"); return ESP_OK; }
    struct stat st;
    if (stat(safe, &st) != 0 || S_ISDIR(st.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found"); return ESP_OK;
    }
    FILE *f = fopen(safe, "rb");
    if (!f) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Open failed"); return ESP_OK; }
    const char *filename = strrchr(safe, '/');
    filename = filename ? filename + 1 : safe;
    char disp[128];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", disp);
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, (ssize_t)n) != ESP_OK) break;
    }
    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

static esp_err_t handleSdFormat(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;
    bool ok = storageMgr.formatSdCard();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok ? "{\"ok\":true}" : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleGpxImport(httpd_req_t *req) {
    if (!checkAuth(req)) return ESP_OK;

    int contentLen = req->content_len;
    if (contentLen <= 0 || contentLen > 65536) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_OK;
    }

    // Heap-allocate: GpxImporter contains a 64KB buffer
    GpxImporter *importer = new GpxImporter();
    if (!importer) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    char chunk[512];
    int  remaining = contentLen;
    while (remaining > 0) {
        int toRead = MIN(remaining, (int)sizeof(chunk));
        int r = httpd_req_recv(req, chunk, toRead);
        if (r <= 0) break;
        importer->feed(chunk, r, false);
        remaining -= r;
    }
    importer->feed("", 0, true);
    importer->parse();

    // ── Save marks (skip duplicates by name) ─────────────────────────────────
    Mark existing[MAX_MARKS];
    int  existCount = storageMgr.loadMarks(existing, MAX_MARKS);
    int  marksAdded = 0;

    int newMarkCount = importer->markCount();
    Mark *newMarks   = importer->newMarks;

    for (int i = 0; i < newMarkCount; i++) {
        bool dup = false;
        for (int j = 0; j < existCount; j++) {
            if (strcasecmp(existing[j].name, newMarks[i].name) == 0) {
                // Re-use existing ID so routes can resolve against it
                strlcpy(newMarks[i].id, existing[j].id, sizeof(newMarks[i].id));
                dup = true;
                break;
            }
        }
        if (!dup) {
            storageMgr.generateMarkId(newMarks[i].id, sizeof(newMarks[i].id));
            vTaskDelay(2);  // ensure timer-based IDs are unique
            newMarks[i].created = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            if (storageMgr.saveMark(newMarks[i])) {
                existing[existCount++] = newMarks[i];
                marksAdded++;
            }
        }
    }

    // ── Build and save courses (heap-allocated — 64×Course exceeds stack) ────────
    Course *newCourses  = (Course *)malloc(MAX_COURSES * sizeof(Course));
    Course *existCourses = (Course *)malloc(MAX_COURSES * sizeof(Course));
    if (!newCourses || !existCourses) {
        free(newCourses); free(existCourses);
        delete importer;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }
    int courseCount      = importer->buildCourses(newCourses, MAX_COURSES, existing, existCount);
    int existCourseCount = storageMgr.loadCourses(existCourses, MAX_COURSES);
    int coursesAdded = 0;

    for (int i = 0; i < courseCount; i++) {
        bool dup = false;
        for (int j = 0; j < existCourseCount; j++) {
            if (strcasecmp(existCourses[j].name, newCourses[i].name) == 0) { dup = true; break; }
        }
        if (!dup) {
            storageMgr.generateCourseId(newCourses[i].id, sizeof(newCourses[i].id));
            vTaskDelay(2);
            if (storageMgr.saveCourse(newCourses[i])) coursesAdded++;
        }
    }
    free(newCourses);
    free(existCourses);

    char resp[160];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"marks_found\":%d,\"marks_added\":%d,\"routes_found\":%d,\"courses_added\":%d}",
             newMarkCount, marksAdded, importer->routeCount(), coursesAdded);
    delete importer;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ── Track recording handlers ──────────────────────────────────────────────────

static esp_err_t handleTrackStatus(httpd_req_t *req) {
    char buf[640];
    uint32_t histSec = 0;
    if (trackRec.loopFirstTs() && trackRec.loopLastTs() > trackRec.loopFirstTs())
        histSec = trackRec.loopLastTs() - trackRec.loopFirstTs();

    // Free space on the active storage backend (0 if unavailable)
    uint64_t sdTotal = 0, sdUsed = 0;
    uint64_t sdFreeKB = 0;
    if (storageMgr.mounted && storageMgr.isSdCard() &&
        storageMgr.getInfo(&sdTotal, &sdUsed))
        sdFreeKB = (sdTotal > sdUsed) ? (sdTotal - sdUsed) / 1024 : 0;

    snprintf(buf, sizeof(buf),
        "{\"sdAvailable\":%s,\"sdFreeKB\":%llu"
        ",\"loopRunning\":%s,\"count\":%u,\"maxPoints\":%u"
        ",\"firstTs\":%u,\"lastTs\":%u,\"historySec\":%u"
        ",\"segActive\":%s,\"segStartTs\":%u"
        ",\"fileReady\":%s,\"lastFile\":\"%s\""
        ",\"gpsTime\":%u,\"intervalSec\":%u,\"loopHours\":%u}",
        trackRec.sdAvailable ? "true" : "false", sdFreeKB,
        trackRec.loopRunning ? "true" : "false",
        trackRec.loopCount(), trackRec.loopMaxPts(),
        trackRec.loopFirstTs(), trackRec.loopLastTs(), histSec,
        trackRec.segActive ? "true" : "false", trackRec.segStartTs,
        trackRec.fileReady ? "true" : "false",
        trackRec.fileReady ? trackRec.lastFileName() : "",
        gpsUnixTime, trackRec.intervalSec(), trackRec.loopHours());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleTrackLoopStart(httpd_req_t *req) {
    if (!trackRec.sdAvailable) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"no SD card\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    trackRec.loopRunning = true;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handleTrackLoopStop(httpd_req_t *req) {
    trackRec.loopRunning = false;
    trackRec.segActive   = false;  // abort any open segment
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /tracks/segment/start — mark current GPS time as segment start
static esp_err_t handleTrackSegStart(httpd_req_t *req) {
    if (!trackRec.sdAvailable) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"no SD card\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    if (gpsUnixTime == 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"no GPS time\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    trackRec.segActive   = true;
    trackRec.segStartTs  = gpsUnixTime;
    trackRec.fileReady   = false;
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"startTs\":%u}", gpsUnixTime);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /tracks/segment/stop — export loop slice from segStartTs..now, write GPX
static esp_err_t handleTrackSegStop(httpd_req_t *req) {
    if (!trackRec.segActive) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"no active segment\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    // Require at least 2 MB free so the GPX file has room to be written.
    uint64_t sdTotal = 0, sdUsed = 0;
    if (!storageMgr.mounted || !storageMgr.isSdCard() ||
        !storageMgr.getInfo(&sdTotal, &sdUsed) ||
        (sdTotal - sdUsed) < 2ULL * 1024 * 1024) {
        trackRec.segActive = false;
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"SD card full or unavailable\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    uint32_t t0 = trackRec.segStartTs;
    uint32_t t1 = gpsUnixTime ? gpsUnixTime : t0;
    trackRec.segActive = false;
    bool ok = trackRec.exportSegment(t0, t1);
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":%s,\"file\":\"%s\"}",
             ok ? "true" : "false",
             ok ? trackRec.lastFileName() : "");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /tracks/segment/export — export loop slice with explicit {t0, t1} unix timestamps
static esp_err_t handleTrackExportSegment(httpd_req_t *req) {
    // Reject if loop file itself isn't healthy, before touching the SD card check
    if (!trackRec.sdAvailable) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"SD card not available\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    uint64_t sdTotal = 0, sdUsed = 0;
    if (storageMgr.isSdCard() && storageMgr.getInfo(&sdTotal, &sdUsed) &&
        (sdTotal - sdUsed) < 2ULL * 1024 * 1024) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"SD card almost full (< 2 MB free)\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    char body[64] = {};
    int blen = httpd_req_recv(req, body, sizeof(body) - 1);
    if (blen <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"empty body\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    cJSON *j   = cJSON_Parse(body);
    cJSON *jt0 = j ? cJSON_GetObjectItem(j, "t0") : NULL;
    cJSON *jt1 = j ? cJSON_GetObjectItem(j, "t1") : NULL;
    if (!cJSON_IsNumber(jt0) || !cJSON_IsNumber(jt1)) {
        if (j) cJSON_Delete(j);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"t0 and t1 required\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    uint32_t t0 = (uint32_t)jt0->valuedouble;
    uint32_t t1 = (uint32_t)jt1->valuedouble;
    cJSON_Delete(j);
    if (t0 >= t1) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"t0 must be before t1\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    ESP_LOGI("Track", "Export request t0=%u t1=%u loopCount=%u sdAvail=%d",
             t0, t1, trackRec.loopCount(), trackRec.sdAvailable);
    bool ok = trackRec.exportSegment(t0, t1);
    char resp[192];
    if (ok) {
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"file\":\"%s\"}", trackRec.lastFileName());
    } else {
        snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":\"%s\"}", trackRec.exportErr);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /tracks/config — update recording rate and loop duration, persist to NVS
static esp_err_t handleTrackConfig(httpd_req_t *req) {
    char body[128] = {};
    int  len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    cJSON *j = cJSON_Parse(body);
    if (!j) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    cJSON *iv = cJSON_GetObjectItem(j, "intervalSec");
    cJSON *lh = cJSON_GetObjectItem(j, "loopHours");
    uint8_t newInterval = cfgMgr.cfg.trackIntervalSec;
    uint8_t newHours    = cfgMgr.cfg.trackLoopHours;
    if (cJSON_IsNumber(iv)) newInterval = (uint8_t)iv->valueint;
    if (cJSON_IsNumber(lh)) newHours    = (uint8_t)lh->valueint;
    cJSON_Delete(j);

    bool changed = (newInterval != cfgMgr.cfg.trackIntervalSec ||
                    newHours    != cfgMgr.cfg.trackLoopHours);
    cfgMgr.cfg.trackIntervalSec = newInterval;
    cfgMgr.cfg.trackLoopHours   = newHours;
    cfgMgr.save();

    // Send the response BEFORE reconfigure() — zero-filling a large loop file
    // (e.g. 24h @ 1s = 2.7 MB) can take several seconds and the browser will
    // time out the request if we block first.
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);

    if (changed) trackRec.reconfigure(newInterval, newHours);

    return ESP_OK;
}

// ── Web server setup ──────────────────────────────────────────────────────────

static void startWebServer() {
    // ── HTTPS server on port 443 ─────────────────────────────────────────────
    httpd_ssl_config_t ssl_cfg         = HTTPD_SSL_CONFIG_DEFAULT();
    ssl_cfg.httpd.max_open_sockets     = 10; // status poll + race poll + concurrent button clicks
    ssl_cfg.httpd.max_uri_handlers     = 40; // default is 8
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
    reg(webServer, "/marks",           HTTP_GET,  handleGetMarks);    // public — mark list
    reg(webServer, "/marks",           HTTP_POST, handlePostMark);    // auth — add mark
    reg(webServer, "/marks/delete",    HTTP_POST, handleDeleteMark);  // auth — delete mark
    reg(webServer, "/courses",         HTTP_GET,  handleGetCourses);  // public — course list
    reg(webServer, "/storage/info",    HTTP_GET,  handleStorageInfo); // public — partition stats
    reg(webServer, "/gpx/import",      HTTP_POST, handleGpxImport);   // auth — GPX file import
    reg(webServer, "/files/list",      HTTP_GET,  handleFilesList);    // public — directory listing
    reg(webServer, "/files/rename",    HTTP_POST, handleFilesRename);  // auth — rename entry
    reg(webServer, "/files/delete",    HTTP_POST, handleFilesDelete);  // auth — delete entry
    reg(webServer, "/files/copy",      HTTP_POST, handleFilesCopy);    // auth — copy file
    reg(webServer, "/files/download",  HTTP_GET,  handleFilesDownload);// public — download file
    reg(webServer, "/sdcard/format",   HTTP_POST, handleSdFormat);     // auth — format SD card
    reg(webServer, "/race/state",     HTTP_GET,  handleRaceState);    // public — race state
    reg(webServer, "/race/start",     HTTP_POST, handleRaceStart);    // public — arm countdown
    reg(webServer, "/race/stop",      HTTP_POST, handleRaceStop);     // public — reset to idle
    reg(webServer, "/race/sync",      HTTP_POST, handleRaceSync);     // public — snap to nearest minute
    reg(webServer, "/race/duration",  HTTP_POST, handleRaceDuration); // public — set sequence length
    reg(webServer, "/race/startline", HTTP_POST, handleRaceStartLine);// public — set start line end
    reg(webServer, "/race/course",    HTTP_POST, handleRaceCourse);   // public — set active course
    reg(webServer, "/race/nextleg",   HTTP_POST, handleRaceNextLeg);  // public — advance to next mark
    reg(webServer, "/race/prevleg",   HTTP_POST, handleRacePrevLeg);  // public — go back to previous mark
    reg(webServer, "/race/end",          HTTP_POST, handleRaceEnd);      // public — end race, go to stats
    reg(webServer, "/tracks/status",     HTTP_GET,  handleTrackStatus);     // public — track state
    reg(webServer, "/tracks/loop/start", HTTP_POST, handleTrackLoopStart);  // public — start loop
    reg(webServer, "/tracks/loop/stop",  HTTP_POST, handleTrackLoopStop);   // public — stop loop
    reg(webServer, "/tracks/segment/start",  HTTP_POST, handleTrackSegStart);    // public — mark start
    reg(webServer, "/tracks/segment/stop",   HTTP_POST, handleTrackSegStop);     // public — mark stop + export
    reg(webServer, "/tracks/segment/export", HTTP_POST, handleTrackExportSegment);// public — export with t0/t1
    reg(webServer, "/tracks/config",     HTTP_POST, handleTrackConfig);     // auth — save settings

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
    uint32_t lastDiag = millis();
    uint32_t lastDiagBytes = 0;
    unsigned rawLinesLogged = 0;

    for (;;) {
        if (uartNmeaMtx && xSemaphoreTake(uartNmeaMtx, pdMS_TO_TICKS(20)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        int r = uart_read_bytes(UART_NMEA, buf, sizeof(buf), pdMS_TO_TICKS(10));
        if (uartNmeaMtx) xSemaphoreGive(uartNmeaMtx);

        if (r > 0) {
            nmeaBytesRx += (uint32_t)r;
            for (int i = 0; i < r; i++) {
                char c = (char)buf[i];
                if (c == '\n') {
                    if (overflow) {
                        nmeaOverflowDrops++;
                    } else {
                        msg.line[idx] = '\0';
                        if (idx > 5) {
                            if (rawLinesLogged < 8) {
                                ESP_LOGI(TAG, "[NMEA] RX: %s", msg.line);
                                rawLinesLogged++;
                            }
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

        uint32_t now = millis();
        if (now - lastDiag >= 5000) {
            uint32_t bytesNow = nmeaBytesRx;
            ESP_LOGI(TAG, "[NMEA] UART bytes +%u, total %u, lines %u",
                     bytesNow - lastDiagBytes, bytesNow, nmeaLinesRx);
            lastDiagBytes = bytesNow;
            lastDiag = now;
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

    raceData.duration_s = 300;  // 5-minute sequence default

    setenv("TZ", "UTC0", 1);  // mktime must treat broken-down time as UTC
    tzset();

    cfgMgr.load();
    storageMgr.begin();

    // Track recorder needs SD card mounted — open loop file after storageMgr.begin()
    {
        char trackDir[64];
        snprintf(trackDir, sizeof(trackDir), "%s/tracks", storageMgr.mountPoint());
        trackRec.begin(trackDir,
                       cfgMgr.cfg.trackIntervalSec,
                       cfgMgr.cfg.trackLoopHours);
    }

    telemetryMtx = xSemaphoreCreateMutex();
    uartNmeaMtx  = xSemaphoreCreateMutex();
    ntripUpdateEnabled();

    // ── UART setup ───────────────────────────────────────────────────────────
    // Order: param_config → set_pin → driver_install.
    // No explicit gpio_set_direction — uart_set_pin handles it.
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

    um982Init(UART_NMEA, cfgMgr.cfg.gpsUpdateRate);
    rtcmUartProbeOk = um982ProbePort(UART_RTCM);

    // ── WiFi ────────────────────────────────────────────────────────────────
    esp_netif_init();
    esp_event_loop_create_default();

    esp_err_t hosted_err = (esp_err_t)esp_hosted_init();
    if (hosted_err == ESP_OK)
        hosted_err = (esp_err_t)esp_hosted_connect_to_slave();
    if (hosted_err != ESP_OK) {
        ESP_LOGE(TAG, "[WiFi] ESP-Hosted transport failed: %s",
                 esp_err_to_name(hosted_err));
        return;
    }

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_err = esp_wifi_init(&wifi_init);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "[WiFi] ESP-Hosted initialization failed: %s",
                 esp_err_to_name(wifi_err));
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    Config &cfg = cfgMgr.cfg;
    {
        if (cfg.apMode || strlen(cfg.wifiSSID) == 0) {
            cfg.apMode = true;
            startAP();
        } else {
            startSTA();
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
