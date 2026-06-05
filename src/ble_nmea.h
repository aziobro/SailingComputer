#pragma once
// ── BLE NMEA GPS broadcast ────────────────────────────────────────────────────
//
// Nordic UART Service (NUS) + HM-10 service using the ESP-IDF NimBLE stack.
// Same wire-protocol as the Arduino version — navigation apps (SW Maps, iNavX)
// connect and receive a live NMEA 0183 sentence stream.
//
// Requires sdkconfig:
//   CONFIG_BT_ENABLED=y
//   CONFIG_BT_NIMBLE_ENABLED=y
// ─────────────────────────────────────────────────────────────────────────────

#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_TAG "BLE"

// ── UUID definitions (128-bit, little-endian byte order) ─────────────────────
// NUS Service  6E400001-B5B3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t nus_svc_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
    0x93,0xf3,0xb3,0xb5,0x01,0x00,0x40,0x6e);
// NUS TX char  6E400003
static const ble_uuid128_t nus_tx_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
    0x93,0xf3,0xb3,0xb5,0x03,0x00,0x40,0x6e);
// NUS RX char  6E400002
static const ble_uuid128_t nus_rx_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
    0x93,0xf3,0xb3,0xb5,0x02,0x00,0x40,0x6e);

// HM-10 Service  0000FFE0-0000-1000-8000-00805F9B34FB
static const ble_uuid128_t hm10_svc_uuid = BLE_UUID128_INIT(
    0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,
    0x00,0x10,0x00,0x00,0xe0,0xff,0x00,0x00);
// HM-10 Char   0000FFE1
static const ble_uuid128_t hm10_chr_uuid = BLE_UUID128_INIT(
    0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,
    0x00,0x10,0x00,0x00,0xe1,0xff,0x00,0x00);

// ── State ─────────────────────────────────────────────────────────────────────
static uint16_t nus_tx_handle   = 0;
static uint16_t hm10_chr_handle = 0;
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool     bleConnected    = false;
static bool     bleEnabled      = false;
static char     ble_dev_name[64] = "SailingComputer";

// ── Forward declarations ──────────────────────────────────────────────────────
static int  ble_gap_cb(struct ble_gap_event *event, void *arg);
static void ble_start_adv(void);

// ── GATT access callback ─────────────────────────────────────────────────────
// RX writes from the phone are accepted but ignored.
static int ble_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

// ── GATT service table ────────────────────────────────────────────────────────
static const struct ble_gatt_svc_def ble_gatt_svcs[] = {
    {   // Nordic UART Service — ArduSimple, SparkFun RTK, iNavX
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   // TX — notify to phone (flags before val_handle per struct order)
                .uuid       = &nus_tx_uuid.u,
                .access_cb  = ble_gatt_access,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &nus_tx_handle,
            },
            {   // RX — write from phone
                .uuid      = &nus_rx_uuid.u,
                .access_cb = ble_gatt_access,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    {   // HM-10 serial service — SW Maps "Generic NMEA (Bluetooth LE)"
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &hm10_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &hm10_chr_uuid.u,
                .access_cb  = ble_gatt_access,
                .flags      = BLE_GATT_CHR_F_NOTIFY |
                              BLE_GATT_CHR_F_WRITE   |
                              BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &hm10_chr_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

// ── Advertising ───────────────────────────────────────────────────────────────
static void ble_start_adv(void) {
    // Main advertising packet: NUS service UUID (GPS apps filter on this)
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128             = &nus_svc_uuid;
    fields.num_uuids128         = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // Scan response: device name (sent on active scan request)
    struct ble_hs_adv_fields rsp = {};
    rsp.name             = (const uint8_t *)ble_dev_name;
    rsp.name_len         = strlen(ble_dev_name);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params params = {};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min  = 0x20;  // 20 ms — responsive discovery
    params.itvl_max  = 0x40;  // 40 ms
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                      &params, ble_gap_cb, NULL);
}

// ── GAP event handler ─────────────────────────────────────────────────────────
static int ble_gap_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ble_conn_handle = event->connect.conn_handle;
                bleConnected    = true;
                ESP_LOGI(BLE_TAG, "Client connected handle=%d", ble_conn_handle);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            bleConnected    = false;
            ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(BLE_TAG, "Client disconnected — restarting advertising");
            ble_start_adv();
            break;
        default:
            break;
    }
    return 0;
}

// ── NimBLE host sync callback ─────────────────────────────────────────────────
static void ble_on_sync(void) {
    ble_svc_gap_device_name_set(ble_dev_name);
    ble_start_adv();
    ESP_LOGI(BLE_TAG, "Advertising as '%s'", ble_dev_name);
}

// ── NimBLE host task ──────────────────────────────────────────────────────────
static void ble_host_task(void *param) {
    nimble_port_run();                // blocks until nimble_port_stop()
    nimble_port_freertos_deinit();
}

// ── Public API ────────────────────────────────────────────────────────────────

static void bleNmeaInit(const char *name) {
    strlcpy(ble_dev_name, name, sizeof(ble_dev_name));

    nimble_port_init();

    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = NULL;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(ble_gatt_svcs);
    ble_gatts_add_svcs(ble_gatt_svcs);

    nimble_port_freertos_init(ble_host_task);

    bleEnabled = true;
    ESP_LOGI(BLE_TAG, "NimBLE initialized");
}

// Send one NMEA sentence (without \r\n) over both BLE services.
static void bleNmeaSend(const char *sentence) {
    if (!bleEnabled || !bleConnected) return;
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    char    buf[128];
    int     len = snprintf(buf, sizeof(buf), "%s\r\n", sentence);
    if (len <= 0 || len >= (int)sizeof(buf)) return;

    if (nus_tx_handle) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
        if (om) ble_gatts_notify_custom(ble_conn_handle, nus_tx_handle, om);
    }
    if (hm10_chr_handle) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
        if (om) ble_gatts_notify_custom(ble_conn_handle, hm10_chr_handle, om);
    }
}
