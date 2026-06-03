#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <math.h>
#include "config.h"
#include "webui.h"
#include "um982.h"
#include "ble_nmea.h"

// Set to true to echo raw Serial1 bytes to debug console (helps verify UM982 is talking)
#define DEBUG_NMEA_RAW false

// ── State ─────────────────────────────────────────────────────────────────────

ConfigManager cfgMgr;

// NMEA
static char   nmeaLine[256];
static int    nmeaIdx = 0;
static int    fixQuality = 0;
static int    satCount = 0;
static float  latitude = 0, longitude = 0, heading = 0, sog = 0, cog = 0;
static float  hdop = 0, altitude = 0;
static float  roll = 0;          // labeled "Roll" — pitch of athwartships baseline = boat heel
static float  cogFiltered = 0;   // smoothed COG using circular EMA; frozen below COG_MIN_SOG_KTS
static bool   cogInitialized = false; // true once cogFiltered has been seeded
static bool   hdtValid = false;
static bool   rollValid = false;

// Apply the user-configured heading offset (loaded from NVS, default 90°)
static float applyHeadingOffset(float h) {
    h += cfgMgr.cfg.headingOffset;
    while (h >= 360.0f) h -= 360.0f;
    while (h <    0.0f) h += 360.0f;
    return h;
}

// TCP NMEA server
static WiFiServer nmeaServer(NMEA_TCP_PORT);
#define MAX_NMEA_CLIENTS 4
static WiFiClient nmeaClients[MAX_NMEA_CLIENTS];

// NTRIP
static WiFiClient ntripClient;
static bool       ntripConnected   = false;
static uint32_t   ntripLastAttempt = 0;
static uint32_t   ntripBytesIn     = 0;
static int        ntripActiveIdx   = 0;   // which source we're currently using
static int        ntripFailCount   = 0;   // consecutive failures on active source
static bool       ntripAnyEnabled  = false; // cached: at least one source is configured

// Web server
static WebServer webServer(80);

// WiFi
static bool staConnected = false;

// ── Base64 ────────────────────────────────────────────────────────────────────

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String base64Encode(const String& in) {
    String out;
    int i = 0;
    uint8_t buf3[3], buf4[4];
    int len = in.length();
    const char* s = in.c_str();
    while (len--) {
        buf3[i++] = (uint8_t)*s++;
        if (i == 3) {
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;
            for (int k = 0; k < 4; k++) out += b64chars[buf4[k]];
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++) buf3[j] = 0;
        buf4[0] = (buf3[0] & 0xfc) >> 2;
        buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
        buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
        for (int k = 0; k < i + 1; k++) out += b64chars[buf4[k]];
        while (i++ < 3) out += '=';
    }
    return out;
}

// ── NMEA helpers ──────────────────────────────────────────────────────────────

// Convert NMEA DDMM.MMMMM to decimal degrees
static float nmeaToDeg(float val) {
    int deg = (int)(val / 100);
    float min = val - deg * 100.0f;
    return deg + min / 60.0f;
}

static void parseGGA(const char* s) {
    char buf[256];
    strlcpy(buf, s, sizeof(buf));
    char* tok = strtok(buf, ",");
    int field = 0;
    char latHemi = 'N', lonHemi = 'E';
    while (tok) {
        field++;
        switch (field) {
            case 3: latitude   = nmeaToDeg(atof(tok)); break;
            case 4: latHemi    = tok[0]; break;
            case 5: longitude  = nmeaToDeg(atof(tok)); break;
            case 6: lonHemi    = tok[0]; break;
            case 7: fixQuality = atoi(tok); break;
            case 8: satCount   = atoi(tok); break;
            case 9: hdop       = atof(tok); break;
            case 10: altitude  = atof(tok); break;
        }
        tok = strtok(nullptr, ",");
    }
    if (latHemi == 'S') latitude  = -latitude;
    if (lonHemi == 'W') longitude = -longitude;
}

static void parseHDT(const char* s) {
    char buf[256];
    strlcpy(buf, s, sizeof(buf));
    char* tok = strtok(buf, ",");
    int field = 0;
    while (tok) {
        field++;
        if (field == 2 && strlen(tok) > 0) {
            heading  = applyHeadingOffset(atof(tok));
            hdtValid = true;
        }
        tok = strtok(nullptr, ",");
    }
}

// Apply speed-weighted circular EMA to COG.
// Uses sin/cos averaging to correctly handle the 359°→0° wrap.
// Below COG_MIN_SOG_KTS the filter is frozen — GPS position noise at low speed
// causes wild COG swings that are meaningless for navigation.
static void updateCOG(float rawCog, float speedKts) {
    if (speedKts < cfgMgr.cfg.cogMinSog) return; // frozen — speed below configured threshold

    if (!cogInitialized) {
        cogFiltered    = rawCog;
        cogInitialized = true;
        return;
    }

    // Alpha scales linearly from COG_ALPHA_MIN at minimum speed to
    // COG_ALPHA_MAX at COG_FAST_SOG_KTS and above.
    float minSog = cfgMgr.cfg.cogMinSog;
    float t      = (speedKts - minSog) / (COG_FAST_SOG_KTS - minSog);
    float alpha = COG_ALPHA_MIN + constrain(t, 0.0f, 1.0f) * (COG_ALPHA_MAX - COG_ALPHA_MIN);

    // Circular EMA — blend sin/cos components then recover the angle.
    float rawRad  = rawCog    * DEG_TO_RAD;
    float filtRad = cogFiltered * DEG_TO_RAD;
    float sinBlend = alpha * sinf(rawRad)  + (1.0f - alpha) * sinf(filtRad);
    float cosBlend = alpha * cosf(rawRad)  + (1.0f - alpha) * cosf(filtRad);
    cogFiltered = atan2f(sinBlend, cosBlend) * RAD_TO_DEG;
    if (cogFiltered < 0.0f) cogFiltered += 360.0f;
}

static void parseVTG(const char* s) {
    char buf[256];
    strlcpy(buf, s, sizeof(buf));
    char* tok = strtok(buf, ",");
    int field = 0;
    float rawCog = cog; // keep previous value if field is empty
    while (tok) {
        field++;
        if (field == 2) rawCog = atof(tok);  // true course
        if (field == 6) sog    = atof(tok);  // speed in knots
        tok = strtok(nullptr, ",");
    }
    cog = rawCog;               // raw value (kept for reference)
    updateCOG(rawCog, sog);     // update filtered COG
}

// Parse Unicore #HEADINGA message for heading + pitch
// Format: #HEADINGA,...;<sol_status>,<pos_type>,<baseline>,<heading>,<pitch>,...*checksum
static void parseHEADINGA(const char* line) {
    // Find the data section after the semicolon
    const char* data = strchr(line, ';');
    if (!data) return;
    data++; // skip ';'

    char buf[256];
    strlcpy(buf, data, sizeof(buf));
    // Strip trailing checksum (*xx)
    char* star = strchr(buf, '*');
    if (star) *star = '\0';

    char* tok = strtok(buf, ",");
    int field = 0;
    while (tok) {
        field++;
        switch (field) {
            case 1: // sol_status — must be SOL_COMPUTED
                if (strncmp(tok, "SOL_COMPUTED", 12) != 0) { rollValid = false; return; }
                break;
            case 4: // heading — apply antenna mounting offset
                heading  = applyHeadingOffset(atof(tok));
                hdtValid = true;
                break;
            case 5: // "pitch" of athwartships baseline = boat roll/heel
                roll      = atof(tok);
                rollValid = true;
                break;
        }
        tok = strtok(nullptr, ",");
    }
}

static void processNmeaLine(const char* line) {
    if      (strncmp(line, "$GNGGA",    6) == 0 || strncmp(line, "$GPGGA",    6) == 0) parseGGA(line);
    else if (strncmp(line, "$GNHDT",    6) == 0 || strncmp(line, "$GPHDT",    6) == 0) parseHDT(line);
    else if (strncmp(line, "$GNVTG",    6) == 0 || strncmp(line, "$GPVTG",    6) == 0) parseVTG(line);
    else if (strncmp(line, "#HEADINGA", 9) == 0)                                        parseHEADINGA(line);
}

// ── NMEA TCP broadcast ────────────────────────────────────────────────────────

static void broadcastNmea(const char* line) {
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (nmeaClients[i] && nmeaClients[i].connected()) {
            nmeaClients[i].print(line);
            nmeaClients[i].print("\r\n");
        }
    }
}

// Calculate NMEA checksum (XOR of all bytes between $ and *)
static uint8_t nmeaChecksum(const char* s) {
    uint8_t cs = 0;
    if (*s == '$') s++;
    while (*s && *s != '*') cs ^= (uint8_t)*s++;
    return cs;
}

// Build a corrected $GPHDT sentence using the already-offset heading value
static void broadcastHDT() {
    if (!hdtValid) return;
    char body[32];
    snprintf(body, sizeof(body), "GPHDT,%.4f,T", heading);
    char sentence[48];
    snprintf(sentence, sizeof(sentence), "$%s*%02X", body, nmeaChecksum(body));
    broadcastNmea(sentence);
}

static void acceptNmeaClients() {
    WiFiClient c = nmeaServer.accept();
    if (!c) return;
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
        if (!nmeaClients[i] || !nmeaClients[i].connected()) {
            nmeaClients[i] = c;
            Serial.printf("[NMEA] Client %d: %s\n", i, c.remoteIP().toString().c_str());
            return;
        }
    }
    c.stop();
}

// ── NTRIP ─────────────────────────────────────────────────────────────────────

// Recompute ntripAnyEnabled — call after config load or save.
static void ntripUpdateEnabled() {
    ntripAnyEnabled = false;
    ntripActiveIdx  = 0;
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        if (cfgMgr.cfg.ntrip[i].enabled && strlen(cfgMgr.cfg.ntrip[i].host) > 0) {
            if (!ntripAnyEnabled)
                ntripActiveIdx = i;  // start on the first enabled source
            ntripAnyEnabled = true;
        }
    }
    if (!ntripAnyEnabled)
        Serial.println("[NTRIP] No sources configured — set up NTRIP in the Configuration tab");
    else
        Serial.printf("[NTRIP] %d source(s) enabled — starting on source %d\n",
                      [&]{ int n=0; for(int i=0;i<NTRIP_SOURCES;i++) if(cfgMgr.cfg.ntrip[i].enabled && strlen(cfgMgr.cfg.ntrip[i].host)>0) n++; return n; }(),
                      ntripActiveIdx);
}

// Find the next enabled source starting after idx (wraps around)
static int ntripNextSource(int idx) {
    for (int i = 1; i <= NTRIP_SOURCES; i++) {
        int next = (idx + i) % NTRIP_SOURCES;
        if (cfgMgr.cfg.ntrip[next].enabled && strlen(cfgMgr.cfg.ntrip[next].host) > 0)
            return next;
    }
    return -1; // none available
}

static void ntripDisconnect() {
    if (ntripClient.connected()) ntripClient.stop();
    ntripConnected = false;
}

static bool ntripConnect(int idx) {
    NtripSource& src = cfgMgr.cfg.ntrip[idx];
    if (!src.enabled || strlen(src.host) == 0) return false;

    IPAddress ntripIP;
    if (!WiFi.hostByName(src.host, ntripIP)) {
        Serial.printf("[NTRIP%d] DNS failed for '%s'\n", idx, src.host);
        return false;
    }
    Serial.printf("[NTRIP%d] Connecting to %s (%s):%d/%s\n",
                  idx, src.host, ntripIP.toString().c_str(), src.port, src.mount);

    if (!ntripClient.connect(ntripIP, src.port)) {
        Serial.printf("[NTRIP%d] TCP connect failed\n", idx);
        return false;
    }

    String req = "GET /";
    req += src.mount;
    req += " HTTP/1.0\r\nHost: ";
    req += src.host;
    req += "\r\nNtrip-Version: Ntrip/2.0\r\nUser-Agent: NTRIP SailingComputer/1.0\r\n";
    if (strlen(src.user) > 0) {
        req += "Authorization: Basic ";
        req += base64Encode(String(src.user) + ":" + String(src.pass));
        req += "\r\n";
    }
    req += "\r\n";
    ntripClient.print(req);

    uint32_t t = millis();
    while (ntripClient.connected() && millis() - t < 5000) {
        if (ntripClient.available()) {
            String line = ntripClient.readStringUntil('\n');
            Serial.printf("[NTRIP%d] < %s\n", idx, line.c_str());
            if (line.indexOf("200") >= 0) {
                while (ntripClient.available()) {
                    line = ntripClient.readStringUntil('\n');
                    if (line.length() <= 2) break;
                }
                ntripConnected = true;
                ntripFailCount = 0;
                Serial.printf("[NTRIP%d] Connected OK\n", idx);
                return true;
            }
            if (line.indexOf("401") >= 0) {
                Serial.printf("[NTRIP%d] Auth failed\n", idx);
                ntripClient.stop();
                return false;
            }
        }
    }
    ntripClient.stop();
    Serial.printf("[NTRIP%d] No valid response\n", idx);
    return false;
}

static void ntripLoop() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (!ntripAnyEnabled) return;

    if (!ntripConnected) {
        // If the active source is disabled or has no host, skip to the next
        // enabled source immediately — don't burn a reconnect timeout on it.
        NtripSource& active = cfgMgr.cfg.ntrip[ntripActiveIdx];
        if (!active.enabled || strlen(active.host) == 0) {
            int next = ntripNextSource(ntripActiveIdx);
            if (next >= 0) {
                Serial.printf("[NTRIP] Source %d disabled — switching to source %d\n",
                              ntripActiveIdx, next);
                ntripActiveIdx = next;
                ntripFailCount = 0;
                ntripLastAttempt = 0;  // attempt immediately
            }
            return;
        }

        uint32_t now = millis();
        if (now - ntripLastAttempt < NTRIP_RECONNECT_MS) return;
        ntripLastAttempt = now;

        // After NTRIP_FAILOVER_COUNT consecutive connection failures, try next source
        if (ntripFailCount >= NTRIP_FAILOVER_COUNT) {
            int next = ntripNextSource(ntripActiveIdx);
            if (next >= 0 && next != ntripActiveIdx) {
                Serial.printf("[NTRIP] Failing over from source %d to %d\n",
                              ntripActiveIdx, next);
                ntripActiveIdx = next;
            }
            ntripFailCount = 0;
        }

        if (!ntripConnect(ntripActiveIdx)) {
            ntripFailCount++;
        }
        return;
    }

    if (!ntripClient.connected()) {
        Serial.printf("[NTRIP%d] Connection dropped\n", ntripActiveIdx);
        ntripDisconnect();
        ntripFailCount++;
        return;
    }

    int avail = ntripClient.available();
    if (avail > 0) {
        uint8_t buf[256];
        int n = ntripClient.read(buf, min(avail, (int)sizeof(buf)));
        if (n > 0) {
            Serial2.write(buf, n);
            ntripBytesIn += n;
            static uint32_t lastRtcmLog = 0;
            if (millis() - lastRtcmLog > 5000) {
                Serial.printf("[NTRIP%d] RTCM flowing — total %u bytes\n",
                              ntripActiveIdx, ntripBytesIn);
                lastRtcmLog = millis();
            }
        }
    }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

static void startAP() {
    Config& cfg = cfgMgr.cfg;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(cfg.apSSID, cfg.apPassword);
    Serial.printf("[WiFi] AP: %s  IP: %s\n", cfg.apSSID,
                  WiFi.softAPIP().toString().c_str());
}

static void startSTA() {
    Config& cfg = cfgMgr.cfg;
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSSID, cfg.wifiPassword);
    Serial.printf("[WiFi] Connecting to %s...\n", cfg.wifiSSID);
}

// ── Web handlers ──────────────────────────────────────────────────────────────

static void handleRoot() {
    webServer.send(200, "text/html", getWebUI());
}

static void handleStatus() {
    const char* fixLabel[] = {
        "No Fix","GPS","DGPS","PPS","RTK Fixed","RTK Float",
        "Dead Reck","Manual","Sim","WAAS"
    };
    String json = "{";
    json += "\"fix\":"           + String(fixQuality) + ",";
    json += "\"fixLabel\":\""    + String(fixQuality <= 9 ? fixLabel[fixQuality] : "Unknown") + "\",";
    json += "\"lat\":"           + String(latitude,  7) + ",";
    json += "\"lon\":"           + String(longitude, 7) + ",";
    json += "\"heading\":"       + String(heading, 2) + ",";
    json += "\"hdtValid\":"      + String(hdtValid ? "true" : "false") + ",";
    json += "\"sog\":"           + String(sog, 2) + ",";
    json += "\"cog\":"           + String(cogFiltered, 1) + ",";
    json += "\"cogValid\":"      + String(cogInitialized && sog >= cfgMgr.cfg.cogMinSog ? "true" : "false") + ",";
    json += "\"cogMinSog\":"     + String(cfgMgr.cfg.cogMinSog, 2) + ",";
    json += "\"sats\":"          + String(satCount) + ",";
    json += "\"hdop\":"          + String(hdop, 2) + ",";
    json += "\"altitude\":"      + String(altitude, 2) + ",";
    json += "\"roll\":"          + String(roll, 2) + ",";
    json += "\"rollValid\":"     + String(rollValid ? "true" : "false") + ",";
    json += "\"bleEnabled\":"     + String(bleEnabled     ? "true" : "false") + ",";
    json += "\"bleConnected\":"   + String(bleConnected   ? "true" : "false") + ",";
    json += "\"ntripConnected\":" + String(ntripConnected ? "true" : "false") + ",";
    json += "\"ntripActiveIdx\":" + String(ntripActiveIdx) + ",";
    json += "\"ntripBytesIn\":"  + String(ntripBytesIn) + ",";
    json += "\"wifiMode\":\""    + String(cfgMgr.cfg.apMode ? "AP" : "Station") + "\",";
    json += "\"ip\":\""          + WiFi.localIP().toString() + "\",";
    json += "\"apIP\":\""        + WiFi.softAPIP().toString() + "\"";
    json += "}";
    webServer.send(200, "application/json", json);
}

static void handleGetConfig() {
    Config& cfg = cfgMgr.cfg;
    String json = "{";
    json += "\"apMode\":"     + String(cfg.apMode ? "true" : "false") + ",";
    json += "\"wifiSSID\":\"" + String(cfg.wifiSSID) + "\",";
    json += "\"ntrip\":[";
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        if (i) json += ",";
        json += "{";
        json += "\"enabled\":"  + String(cfg.ntrip[i].enabled ? "true" : "false") + ",";
        json += "\"host\":\""   + String(cfg.ntrip[i].host)  + "\",";
        json += "\"port\":"     + String(cfg.ntrip[i].port)  + ",";
        json += "\"mount\":\"" + String(cfg.ntrip[i].mount) + "\",";
        json += "\"user\":\""   + String(cfg.ntrip[i].user)  + "\"";
        json += "}";
    }
    json += "],";
    json += "\"headingOffset\":" + String(cfg.headingOffset, 1) + ",";
    json += "\"cogMinSog\":"     + String(cfg.cogMinSog, 2) + ",";
    json += "\"bleNmea\":"       + String(cfg.bleNmea ? "true" : "false") + ",";
    json += "\"apSSID\":\"" + String(cfg.apSSID) + "\"";
    json += "}";
    webServer.send(200, "application/json", json);
}

static void handleSaveConfig() {
    Config& cfg = cfgMgr.cfg;
    if (webServer.hasArg("apMode"))   cfg.apMode = webServer.arg("apMode") == "true";
    if (webServer.hasArg("wifiSSID")) strlcpy(cfg.wifiSSID, webServer.arg("wifiSSID").c_str(), sizeof(cfg.wifiSSID));
    if (webServer.hasArg("wifiPassword")) { String v = webServer.arg("wifiPassword"); if (v.length() > 0 && v != "(unchanged)") strlcpy(cfg.wifiPassword, v.c_str(), sizeof(cfg.wifiPassword)); }

    char key[16];
    for (int i = 0; i < NTRIP_SOURCES; i++) {
        snprintf(key, sizeof(key), "n%denabled", i);
        cfg.ntrip[i].enabled = webServer.hasArg(key) &&
                               (webServer.arg(key) == "true" || webServer.arg(key) == "on");

        snprintf(key, sizeof(key), "n%dhost", i);
        if (webServer.hasArg(key)) { String v = webServer.arg(key); if (v.length() > 0) strlcpy(cfg.ntrip[i].host, v.c_str(), sizeof(cfg.ntrip[i].host)); }

        snprintf(key, sizeof(key), "n%dport", i);
        if (webServer.hasArg(key)) cfg.ntrip[i].port = (uint16_t)webServer.arg(key).toInt();

        snprintf(key, sizeof(key), "n%dmount", i);
        if (webServer.hasArg(key)) { String v = webServer.arg(key); if (v.length() > 0) strlcpy(cfg.ntrip[i].mount, v.c_str(), sizeof(cfg.ntrip[i].mount)); }

        snprintf(key, sizeof(key), "n%duser", i);
        if (webServer.hasArg(key)) { String v = webServer.arg(key); if (v.length() > 0) strlcpy(cfg.ntrip[i].user, v.c_str(), sizeof(cfg.ntrip[i].user)); }

        snprintf(key, sizeof(key), "n%dpass", i);
        if (webServer.hasArg(key)) { String v = webServer.arg(key); if (v.length() > 0 && v != "(unchanged)") strlcpy(cfg.ntrip[i].pass, v.c_str(), sizeof(cfg.ntrip[i].pass)); }

        Serial.printf("[Config] NTRIP%d: enabled:%d host:'%s' port:%d mount:'%s'\n",
                      i, cfg.ntrip[i].enabled, cfg.ntrip[i].host, cfg.ntrip[i].port, cfg.ntrip[i].mount);
    }

    if (webServer.hasArg("headingOffset")) cfg.headingOffset = webServer.arg("headingOffset").toFloat();
    if (webServer.hasArg("cogMinSog"))  cfg.cogMinSog = webServer.arg("cogMinSog").toFloat();
    if (webServer.hasArg("bleNmea")) {
        String v = webServer.arg("bleNmea");
        cfg.bleNmea = (v == "true" || v == "on" || v == "1");
    } else {
        cfg.bleNmea = false;
    }
    if (webServer.hasArg("apSSID"))    strlcpy(cfg.apSSID, webServer.arg("apSSID").c_str(), sizeof(cfg.apSSID));
    if (webServer.hasArg("apPassword")) { String v = webServer.arg("apPassword"); if (v.length() > 0 && v != "(unchanged)") strlcpy(cfg.apPassword, v.c_str(), sizeof(cfg.apPassword)); }
    cfgMgr.save();
    ntripUpdateEnabled();
    ntripDisconnect();
    ntripActiveIdx = 0;
    ntripFailCount = 0;
    webServer.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
}

static void handleUM982Reset() {
    webServer.send(200, "application/json", "{\"ok\":true}");
    Serial.println("[Web] UM982 factory reset requested");
    delay(100);
    um982FactoryReset(Serial1);
}

static void handleBleToggle() {
    if (webServer.hasArg("bleNmea")) {
        String v = webServer.arg("bleNmea");
        cfgMgr.cfg.bleNmea = (v == "true" || v == "on" || v == "1");
        cfgMgr.save();
        Serial.printf("[BLE] %s — restarting\n", cfgMgr.cfg.bleNmea ? "enabled" : "disabled");
    }
    webServer.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
}

static void handleRestart() {
    webServer.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
}

// ── OTA firmware update ────────────────────────────────────────────────────────

static bool otaError = false;

// Receives the binary upload chunk by chunk and writes it to flash.
static void handleOTAUpload() {
    HTTPUpload& upload = webServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        otaError = false;
        Serial.printf("[OTA] Receiving: %s (%u bytes)\n",
                      upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            otaError = true;
            Serial.printf("[OTA] begin() failed: %s\n", Update.errorString());
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!otaError) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                otaError = true;
                Serial.printf("[OTA] write() failed: %s\n", Update.errorString());
            }
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (!otaError) {
            if (!Update.end(true)) {
                otaError = true;
                Serial.printf("[OTA] end() failed: %s\n", Update.errorString());
            } else {
                Serial.printf("[OTA] Flash complete — %u bytes written\n", upload.totalSize);
            }
        }
    }
}

// Called after upload completes — sends result page then reboots on success.
static void handleOTAComplete() {
    if (otaError) {
        webServer.send(500, "text/html",
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>OTA Failed</title>"
            "<style>body{font-family:system-ui;background:#0a1628;color:#e0e8f0;"
            "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}"
            ".box{background:#0d1f3c;border:1px solid #1e3a5f;border-radius:8px;padding:2rem;max-width:400px;text-align:center;}"
            "h2{color:#ff6b6b;} a{color:#4a9eff;}</style></head>"
            "<body><div class='box'><h2>&#10007; Update Failed</h2>"
            "<p>" + String(Update.errorString()) + "</p>"
            "<p><a href='/'>Back to dashboard</a></p>"
            "</div></body></html>");
    } else {
        webServer.send(200, "text/html",
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>OTA Success</title>"
            "<style>body{font-family:system-ui;background:#0a1628;color:#e0e8f0;"
            "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;}"
            ".box{background:#0d1f3c;border:1px solid #1e3a5f;border-radius:8px;padding:2rem;max-width:400px;text-align:center;}"
            "h2{color:#4ade80;} .note{color:#8899aa;font-size:0.85rem;}"
            "</style>"
            "<script>setTimeout(()=>location.href='/',9000)</script>"
            "</head><body><div class='box'>"
            "<h2>&#10003; Update Successful</h2>"
            "<p>Firmware flashed. Device is restarting&hellip;</p>"
            "<p class='note'>Redirecting to dashboard in 9 seconds.</p>"
            "</div></body></html>");
        delay(500);
        ESP.restart();
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Boot] SailingComputer starting...");

    cfgMgr.load();
    ntripUpdateEnabled();

    Serial1.begin(NMEA_BAUD, SERIAL_8N1, SERIAL1_RX, SERIAL1_TX);
    Serial2.begin(RTCM_BAUD, SERIAL_8N1, SERIAL2_RX, SERIAL2_TX);
    Serial.println("[Boot] UART initialized");

    um982Init(Serial1);

    if (cfgMgr.cfg.bleNmea)
        bleNmeaInit("SailingComputer");

    Config& cfg = cfgMgr.cfg;

    if (cfg.apMode || strlen(cfg.wifiSSID) == 0) {
        cfg.apMode = true;
        startAP();
    } else {
        startSTA();
        Serial.print("[WiFi] Waiting");
        uint32_t wifiStart = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            staConnected = true;
            Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
            ntripLastAttempt = millis(); // delay first NTRIP attempt by NTRIP_RECONNECT_MS
        } else {
            Serial.println("[WiFi] STA failed, falling back to AP");
            cfg.apMode = true;
            startAP();
        }
    }

    if (MDNS.begin("sailingcomputer"))
        Serial.println("[mDNS] sailingcomputer.local");

    nmeaServer.begin();
    Serial.printf("[NMEA] TCP server port %d\n", NMEA_TCP_PORT);

    webServer.on("/",            HTTP_GET,  handleRoot);
    webServer.on("/status",      HTTP_GET,  handleStatus);
    webServer.on("/config",      HTTP_GET,  handleGetConfig);
    webServer.on("/config/save", HTTP_POST, handleSaveConfig);
    webServer.on("/restart",     HTTP_POST, handleRestart);
    webServer.on("/um982reset",  HTTP_POST, handleUM982Reset);
    webServer.on("/update",      HTTP_POST, handleOTAComplete, handleOTAUpload);
    webServer.on("/ble/toggle",  HTTP_POST, handleBleToggle);
    webServer.begin();
    Serial.println("[Web] HTTP server started");
    Serial.println("[Boot] Ready");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    webServer.handleClient();
    acceptNmeaClients();

    // Read NMEA from UM982 COM1
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        if (DEBUG_NMEA_RAW && (c >= 0x20 || c == '\n' || c == '\r')) Serial.write(c);
        if (c == '\n') {
            nmeaLine[nmeaIdx] = '\0';
            if (nmeaIdx > 5) {
                processNmeaLine(nmeaLine);
                // Only broadcast standard $ NMEA sentences — skip proprietary # Unicore messages
                if (nmeaLine[0] == '$') {
                    // Replace raw $GPHDT with a corrected-heading sentence
                    if (strncmp(nmeaLine, "$GPHDT", 6) == 0 ||
                        strncmp(nmeaLine, "$GNHDT", 6) == 0) {
                        broadcastHDT();          // TCP clients
                        if (bleEnabled) {        // BLE clients
                            char body[32], sentence[48];
                            snprintf(body,     sizeof(body),     "GPHDT,%.4f,T", heading);
                            snprintf(sentence, sizeof(sentence), "$%s*%02X", body, nmeaChecksum(body));
                            bleNmeaSend(sentence);
                        }
                    } else {
                        broadcastNmea(nmeaLine);
                        bleNmeaSend(nmeaLine);   // BLE (no-op if not connected)
                    }
                }
            }
            nmeaIdx = 0;
        } else if (c != '\r' && nmeaIdx < (int)sizeof(nmeaLine) - 1) {
            nmeaLine[nmeaIdx++] = c;
        }
    }

    ntripLoop();

    // Monitor STA reconnection
    if (!cfgMgr.cfg.apMode) {
        bool connected = (WiFi.status() == WL_CONNECTED);
        if (!connected && staConnected) {
            staConnected = false;
            ntripDisconnect();
            Serial.println("[WiFi] Lost connection");
        } else if (connected && !staConnected) {
            staConnected = true;
            Serial.printf("[WiFi] Reconnected: %s\n", WiFi.localIP().toString().c_str());
        }
    }
}
