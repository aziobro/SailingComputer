#pragma once
#include <Preferences.h>

// Default AP settings
#define DEFAULT_AP_SSID     "SailingComputer"
#define DEFAULT_AP_PASSWORD "sailing123"

// Serial pins
#define SERIAL1_RX 16   // UM982 COM1 TX -> ESP32
#define SERIAL1_TX 17   // ESP32 -> UM982 COM1 RX
#define SERIAL2_RX 19   // UM982 COM2 TX -> ESP32
#define SERIAL2_TX 18   // ESP32 -> UM982 COM2 RX (RTCM in)

#define NMEA_BAUD  115200
#define RTCM_BAUD  115200

// TCP NMEA broadcast port
#define NMEA_TCP_PORT 10110

// NTRIP reconnect interval (ms) and fails before switching source
#define NTRIP_RECONNECT_MS   30000
#define NTRIP_FAILOVER_COUNT 3

// COG smoothing — suppress updates below this speed (knots)
#define COG_MIN_SOG_KTS   0.3f
// EMA alpha range: alpha=MIN at COG_MIN_SOG, alpha=MAX at COG_FAST_SOG and above
// Lower alpha = heavier smoothing (more lag); higher = faster response
#define COG_ALPHA_MIN     0.05f   // heavy smoothing at low speed
#define COG_ALPHA_MAX     0.25f   // light smoothing at high speed
#define COG_FAST_SOG_KTS  3.0f   // speed at which alpha reaches COG_ALPHA_MAX

// STA WiFi connect timeout (ms)
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define NTRIP_SOURCES 3

struct NtripSource {
    char host[128]  = "";
    uint16_t port   = 2101;
    char mount[64]  = "";
    char user[64]   = "";
    char pass[64]   = "";
    bool enabled    = false;
};

struct Config {
    // WiFi
    char wifiSSID[64]     = "";
    char wifiPassword[64] = "";
    bool apMode           = true;

    // NTRIP sources (with failover)
    NtripSource ntrip[NTRIP_SOURCES];

    // Antenna heading offset (degrees added to UM982 heading before use)
    // Default 90° for port/starboard aft-rail mounting (ANT1=stbd, ANT2=port)
    float headingOffset   = 90.0f;

    // AP credentials (editable)
    char apSSID[64]       = DEFAULT_AP_SSID;
    char apPassword[64]   = DEFAULT_AP_PASSWORD;
};

class ConfigManager {
public:
    Config cfg;

    void load() {
        Preferences prefs;
        prefs.begin("sailcomp", true);
        prefs.getString("wifiSSID", cfg.wifiSSID,    sizeof(cfg.wifiSSID));
        prefs.getString("wifiPass", cfg.wifiPassword, sizeof(cfg.wifiPassword));
        cfg.apMode = prefs.getBool("apMode", true);

        // Migrate legacy single-source config
        char legacyHost[128] = "";
        prefs.getString("ntripHost", legacyHost, sizeof(legacyHost));
        if (strlen(legacyHost) > 0 && !prefs.isKey("n0host")) {
            strlcpy(cfg.ntrip[0].host,  legacyHost, sizeof(cfg.ntrip[0].host));
            cfg.ntrip[0].port = prefs.getUShort("ntripPort", 2101);
            prefs.getString("ntripMount", cfg.ntrip[0].mount, sizeof(cfg.ntrip[0].mount));
            prefs.getString("ntripUser",  cfg.ntrip[0].user,  sizeof(cfg.ntrip[0].user));
            prefs.getString("ntripPass",  cfg.ntrip[0].pass,  sizeof(cfg.ntrip[0].pass));
            cfg.ntrip[0].enabled = prefs.getBool("ntripEnabled", false);
        } else {
            char key[16];
            for (int i = 0; i < NTRIP_SOURCES; i++) {
                snprintf(key, sizeof(key), "n%dhost",    i); prefs.getString(key, cfg.ntrip[i].host,  sizeof(cfg.ntrip[i].host));
                snprintf(key, sizeof(key), "n%dport",    i); cfg.ntrip[i].port = prefs.getUShort(key, 2101);
                snprintf(key, sizeof(key), "n%dmount",   i); prefs.getString(key, cfg.ntrip[i].mount, sizeof(cfg.ntrip[i].mount));
                snprintf(key, sizeof(key), "n%duser",    i); prefs.getString(key, cfg.ntrip[i].user,  sizeof(cfg.ntrip[i].user));
                snprintf(key, sizeof(key), "n%dpass",    i); prefs.getString(key, cfg.ntrip[i].pass,  sizeof(cfg.ntrip[i].pass));
                snprintf(key, sizeof(key), "n%denabled", i); cfg.ntrip[i].enabled = prefs.getBool(key, false);
            }
        }

        cfg.headingOffset = prefs.getFloat("hdgOffset", 90.0f);

        prefs.getString("apSSID", cfg.apSSID,     sizeof(cfg.apSSID));
        prefs.getString("apPass", cfg.apPassword, sizeof(cfg.apPassword));
        if (strlen(cfg.apSSID) == 0)     strlcpy(cfg.apSSID,     DEFAULT_AP_SSID,     sizeof(cfg.apSSID));
        if (strlen(cfg.apPassword) == 0) strlcpy(cfg.apPassword, DEFAULT_AP_PASSWORD, sizeof(cfg.apPassword));
        prefs.end();
    }

    void save() {
        Preferences prefs;
        prefs.begin("sailcomp", false);
        prefs.putString("wifiSSID", cfg.wifiSSID);
        prefs.putString("wifiPass", cfg.wifiPassword);
        prefs.putBool  ("apMode",   cfg.apMode);

        char key[16];
        for (int i = 0; i < NTRIP_SOURCES; i++) {
            snprintf(key, sizeof(key), "n%dhost",    i); prefs.putString(key,  cfg.ntrip[i].host);
            snprintf(key, sizeof(key), "n%dport",    i); prefs.putUShort(key,  cfg.ntrip[i].port);
            snprintf(key, sizeof(key), "n%dmount",   i); prefs.putString(key,  cfg.ntrip[i].mount);
            snprintf(key, sizeof(key), "n%duser",    i); prefs.putString(key,  cfg.ntrip[i].user);
            snprintf(key, sizeof(key), "n%dpass",    i); prefs.putString(key,  cfg.ntrip[i].pass);
            snprintf(key, sizeof(key), "n%denabled", i); prefs.putBool(key,    cfg.ntrip[i].enabled);
        }

        prefs.putFloat ("hdgOffset", cfg.headingOffset);
        prefs.putString("apSSID", cfg.apSSID);
        prefs.putString("apPass", cfg.apPassword);
        prefs.end();
    }
};
