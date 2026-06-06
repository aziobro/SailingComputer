#pragma once
#include <string.h>
#include <stdint.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

// Default AP settings
#define DEFAULT_AP_SSID     "SailingComputer"
#define DEFAULT_AP_PASSWORD "sailing123"

// Serial (UART) pins — ESP32-P4
// UART2 (UART_NUM_2) → UM982 COM2 (NMEA/control + RTCM): GPIO32=RX, GPIO33=TX
// UART1 (UART_NUM_1) → UM982 COM1 diagnostic link:       GPIO47=RX, GPIO48=TX
// The COM identities were confirmed from the UM982 HEADINGA port header.
#define SERIAL1_RX 32   // UM982 COM2 TX  → ESP32 GPIO32 (blue)
#define SERIAL1_TX 33   // ESP32 GPIO33   → UM982 COM2 RX (green)
#define SERIAL2_RX 47   // UM982 COM1 TX  → ESP32 GPIO47 (yellow, diagnostic)
#define SERIAL2_TX 48   // ESP32 GPIO48   → UM982 COM1 RX (orange, diagnostic)
#define PPS_PIN    27   // UM982 PPS      → ESP32 GPIO27 (white)

#define NMEA_BAUD  115200
#define RTCM_BAUD  115200

// TCP NMEA broadcast port
#define NMEA_TCP_PORT 10110

// NTRIP reconnect interval (ms) and fails before switching source
#define NTRIP_RECONNECT_MS   30000
#define NTRIP_FAILOVER_COUNT 3

// COG smoothing
#define COG_MIN_SOG_DEFAULT 0.1f
#define COG_ALPHA_MIN       0.05f
#define COG_ALPHA_MAX       0.25f
#define COG_FAST_SOG_KTS    3.0f

// STA WiFi connect timeout (ms)
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define NTRIP_SOURCES 3

static const char* CFG_TAG = "Config";

struct NtripSource {
    char     host[128];
    uint16_t port;
    char     mount[64];
    char     user[64];
    char     pass[64];
    bool     enabled;
};

struct Config {
    char        wifiSSID[64];
    char        wifiPassword[64];
    bool        apMode;
    NtripSource ntrip[NTRIP_SOURCES];
    float       headingOffset;
    float       cogMinSog;
    bool        bleNmea;
    char        apSSID[64];
    char        apPassword[64];
    char        adminPassword[64];  // HTTP Basic Auth for config/OTA pages
};

// Store/load float via its bit pattern — NVS has no native float type.
static inline uint32_t f2u(float f)  { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float    u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

class ConfigManager {
public:
    Config cfg;

    ConfigManager() {
        memset(&cfg, 0, sizeof(cfg));
        cfg.apMode        = true;
        cfg.headingOffset = 90.0f;
        cfg.cogMinSog     = COG_MIN_SOG_DEFAULT;
        for (int i = 0; i < NTRIP_SOURCES; i++) cfg.ntrip[i].port = 2101;
        strlcpy(cfg.apSSID,       DEFAULT_AP_SSID,     sizeof(cfg.apSSID));
        strlcpy(cfg.apPassword,   DEFAULT_AP_PASSWORD, sizeof(cfg.apPassword));
        strlcpy(cfg.adminPassword, "admin",            sizeof(cfg.adminPassword));
    }

    void load() {
        nvs_handle_t h;
        if (nvs_open("sailcomp", NVS_READONLY, &h) != ESP_OK) {
            ESP_LOGI(CFG_TAG, "No saved config — using defaults");
            return;
        }

        size_t  len;
        uint8_t b;
        uint32_t u;

        len = sizeof(cfg.wifiSSID);     nvs_get_str(h, "wifiSSID", cfg.wifiSSID,     &len);
        len = sizeof(cfg.wifiPassword); nvs_get_str(h, "wifiPass", cfg.wifiPassword,  &len);
        if (nvs_get_u8(h, "apMode", &b) == ESP_OK) cfg.apMode = b;

        // Migrate legacy single-source config
        char legacyHost[128] = "";
        len = sizeof(legacyHost);
        nvs_get_str(h, "ntripHost", legacyHost, &len);

        // Check if "n0host" exists with a non-empty value.
        // Pass NULL buf so IDF returns the required size without a copy —
        // avoids the ESP_ERR_NVS_INVALID_LENGTH false-negative that a small
        // fixed buffer would cause for any host longer than 3 chars.
        char n0key[16]; snprintf(n0key, sizeof(n0key), "n%dhost", 0);
        size_t n0len = 0;
        bool hasN0 = (nvs_get_str(h, n0key, NULL, &n0len) == ESP_OK && n0len > 1);

        if (strlen(legacyHost) > 0 && !hasN0) {
            strlcpy(cfg.ntrip[0].host, legacyHost, sizeof(cfg.ntrip[0].host));
            uint16_t port = 2101; nvs_get_u16(h, "ntripPort", &port); cfg.ntrip[0].port = port;
            len = sizeof(cfg.ntrip[0].mount); nvs_get_str(h, "ntripMount", cfg.ntrip[0].mount, &len);
            len = sizeof(cfg.ntrip[0].user);  nvs_get_str(h, "ntripUser",  cfg.ntrip[0].user,  &len);
            len = sizeof(cfg.ntrip[0].pass);  nvs_get_str(h, "ntripPass",  cfg.ntrip[0].pass,  &len);
            b = 0; nvs_get_u8(h, "ntripEnabled", &b); cfg.ntrip[0].enabled = b;
        } else {
            char key[16];
            for (int i = 0; i < NTRIP_SOURCES; i++) {
                snprintf(key, sizeof(key), "n%dhost",    i); len = sizeof(cfg.ntrip[i].host);  nvs_get_str(h, key, cfg.ntrip[i].host,  &len);
                snprintf(key, sizeof(key), "n%dport",    i); { uint16_t p = 2101; nvs_get_u16(h, key, &p); cfg.ntrip[i].port = p; }
                snprintf(key, sizeof(key), "n%dmount",   i); len = sizeof(cfg.ntrip[i].mount); nvs_get_str(h, key, cfg.ntrip[i].mount, &len);
                snprintf(key, sizeof(key), "n%duser",    i); len = sizeof(cfg.ntrip[i].user);  nvs_get_str(h, key, cfg.ntrip[i].user,  &len);
                snprintf(key, sizeof(key), "n%dpass",    i); len = sizeof(cfg.ntrip[i].pass);  nvs_get_str(h, key, cfg.ntrip[i].pass,  &len);
                snprintf(key, sizeof(key), "n%denabled", i); b = 0; nvs_get_u8(h, key, &b); cfg.ntrip[i].enabled = b;
                ESP_LOGD(CFG_TAG, "Load NTRIP%d: en=%d host='%s' port=%d mount='%s' user='%s'",
                         i, cfg.ntrip[i].enabled, cfg.ntrip[i].host, cfg.ntrip[i].port,
                         cfg.ntrip[i].mount, cfg.ntrip[i].user);
            }
        }

        if (nvs_get_u32(h, "hdgOffset", &u) == ESP_OK) cfg.headingOffset = u2f(u);
        if (nvs_get_u32(h, "cogMinSog", &u) == ESP_OK) cfg.cogMinSog     = u2f(u);
        b = 0; nvs_get_u8(h, "bleNmea", &b); cfg.bleNmea = b;

        len = sizeof(cfg.apSSID);       nvs_get_str(h, "apSSID",    cfg.apSSID,       &len);
        len = sizeof(cfg.apPassword);   nvs_get_str(h, "apPass",    cfg.apPassword,   &len);
        len = sizeof(cfg.adminPassword);nvs_get_str(h, "adminPass", cfg.adminPassword,&len);
        if (strlen(cfg.apSSID)       == 0) strlcpy(cfg.apSSID,       DEFAULT_AP_SSID,     sizeof(cfg.apSSID));
        if (strlen(cfg.apPassword)   == 0) strlcpy(cfg.apPassword,   DEFAULT_AP_PASSWORD, sizeof(cfg.apPassword));
        if (strlen(cfg.adminPassword)== 0) strlcpy(cfg.adminPassword,"admin",             sizeof(cfg.adminPassword));

        nvs_close(h);
    }

    void save() {
        nvs_handle_t h;
        if (nvs_open("sailcomp", NVS_READWRITE, &h) != ESP_OK) {
            ESP_LOGE(CFG_TAG, "NVS open for write failed");
            return;
        }

        nvs_set_str(h, "wifiSSID", cfg.wifiSSID);
        nvs_set_str(h, "wifiPass", cfg.wifiPassword);
        nvs_set_u8 (h, "apMode",   cfg.apMode ? 1 : 0);

        char key[16];
        for (int i = 0; i < NTRIP_SOURCES; i++) {
            snprintf(key, sizeof(key), "n%dhost",    i); nvs_set_str(h, key, cfg.ntrip[i].host);
            snprintf(key, sizeof(key), "n%dport",    i); nvs_set_u16(h, key, cfg.ntrip[i].port);
            snprintf(key, sizeof(key), "n%dmount",   i); nvs_set_str(h, key, cfg.ntrip[i].mount);
            snprintf(key, sizeof(key), "n%duser",    i); nvs_set_str(h, key, cfg.ntrip[i].user);
            snprintf(key, sizeof(key), "n%dpass",    i); nvs_set_str(h, key, cfg.ntrip[i].pass);
            snprintf(key, sizeof(key), "n%denabled", i); nvs_set_u8 (h, key, cfg.ntrip[i].enabled ? 1 : 0);
            ESP_LOGD(CFG_TAG, "Save NTRIP%d: en=%d host='%s' port=%d mount='%s' user='%s'",
                     i, cfg.ntrip[i].enabled, cfg.ntrip[i].host, cfg.ntrip[i].port,
                     cfg.ntrip[i].mount, cfg.ntrip[i].user);
        }

        nvs_set_u32(h, "hdgOffset", f2u(cfg.headingOffset));
        nvs_set_u32(h, "cogMinSog", f2u(cfg.cogMinSog));
        nvs_set_u8 (h, "bleNmea",   cfg.bleNmea ? 1 : 0);
        nvs_set_str(h, "apSSID",    cfg.apSSID);
        nvs_set_str(h, "apPass",    cfg.apPassword);
        nvs_set_str(h, "adminPass", cfg.adminPassword);

        nvs_commit(h);
        nvs_close(h);
    }
};
