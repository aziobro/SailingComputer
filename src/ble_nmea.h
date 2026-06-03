#pragma once
// ── BLE NMEA GPS broadcast ────────────────────────────────────────────────────
//
// Implements the Nordic UART Service (NUS) — the de-facto BLE profile used by
// external GPS accessories.  Navigation apps (SW Maps, iNavX, etc.) connect to
// this service and receive a live NMEA 0183 sentence stream, making the ESP32
// appear as a wireless GPS receiver.
//
// Uses NimBLE-Arduino (lighter weight than the classic BLE Arduino library
// and compatible with ESP32 Arduino core 3.x).
//
// Service UUID : 6E400001-B5B3-F393-E0A9-E50E24DCCA9E
// TX char (notify, device→phone) : 6E400003-B5B3-F393-E0A9-E50E24DCCA9E
// RX char (write,  phone→device) : 6E400002-B5B3-F393-E0A9-E50E24DCCA9E
//
// Usage:
//   bleNmeaInit("SailingComputer");  // call once in setup()
//   bleNmeaSend("$GNGGA,...*xx");    // call for each sentence to broadcast
// ─────────────────────────────────────────────────────────────────────────────

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define NUS_SERVICE_UUID  "6E400001-B5B3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_TX_UUID  "6E400003-B5B3-F393-E0A9-E50E24DCCA9E"  // notify → phone
#define NUS_CHAR_RX_UUID  "6E400002-B5B3-F393-E0A9-E50E24DCCA9E"  // write  ← phone

static NimBLEServer*         bleServer    = nullptr;
static NimBLECharacteristic* bleTxChar    = nullptr;
static bool                  bleConnected = false;
static bool                  bleEnabled   = false;

// ── Connection callbacks ──────────────────────────────────────────────────────

class BleServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        bleConnected = true;
        Serial.printf("[BLE] Client connected: %s\n",
                      info.getAddress().toString().c_str());
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
        bleConnected = false;
        Serial.println("[BLE] Client disconnected — restarting advertising");
        NimBLEDevice::startAdvertising();
    }
};

// ── Init ─────────────────────────────────────────────────────────────────────

static void bleNmeaInit(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // max TX power for range on a boat

    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());

    NimBLEService* svc = bleServer->createService(NUS_SERVICE_UUID);

    // TX characteristic — device notifies phone with NMEA sentences
    bleTxChar = svc->createCharacteristic(
        NUS_CHAR_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX characteristic — phone can send commands (reserved for future use)
    svc->createCharacteristic(
        NUS_CHAR_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );

    svc->start();

    // Advertise with the NUS service UUID so GPS apps can find the device
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);
    NimBLEDevice::startAdvertising();

    bleEnabled = true;
    Serial.printf("[BLE] Advertising as '%s'\n", deviceName);
}

// ── Send a single NMEA sentence ───────────────────────────────────────────────
//
// Appends \r\n and sends via BLE notification.  Long sentences are fragmented
// into MTU-sized chunks (rare on modern phones but handled for robustness).

static void bleNmeaSend(const char* sentence) {
    if (!bleEnabled || !bleConnected || !bleTxChar) return;

    char buf[128];
    int  len = snprintf(buf, sizeof(buf), "%s\r\n", sentence);

    // NimBLE negotiates MTU automatically; payload = MTU - 3 overhead bytes.
    // Use a safe default of 182 bytes (185 - 3) which fits all NMEA sentences.
    uint16_t mtu = 182;

    int offset = 0;
    while (offset < len) {
        int chunk = min((int)mtu, len - offset);
        bleTxChar->setValue((uint8_t*)(buf + offset), chunk);
        bleTxChar->notify();
        offset += chunk;
        if (offset < len) delay(10);  // brief gap between fragments
    }
}
