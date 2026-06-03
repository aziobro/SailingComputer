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

// Nordic UART Service — used by ArduSimple, SparkFun RTK, and many GPS apps
#define NUS_SERVICE_UUID  "6E400001-B5B3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_TX_UUID  "6E400003-B5B3-F393-E0A9-E50E24DCCA9E"  // notify → phone
#define NUS_CHAR_RX_UUID  "6E400002-B5B3-F393-E0A9-E50E24DCCA9E"  // write  ← phone

// HM-10 BLE serial UUID — used by many "Generic NMEA BLE" implementations
// including SW Maps "Generic NMEA (Bluetooth LE)" mode
#define HM10_SERVICE_UUID "0000FFE0-0000-1000-8000-00805F9B34FB"
#define HM10_CHAR_UUID    "0000FFE1-0000-1000-8000-00805F9B34FB"  // notify + write

static NimBLEServer*         bleServer    = nullptr;
static NimBLECharacteristic* bleTxChar    = nullptr;   // NUS TX
static NimBLECharacteristic* bleHm10Char  = nullptr;   // HM-10 TX
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
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);     // max TX power for range on a boat
    NimBLEDevice::setSecurityAuth(false, false, false);  // no pairing required

    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());

    // Nordic UART Service (NUS) — ArduSimple, SparkFun RTK, iNavX
    NimBLEService* nusSvc = bleServer->createService(NUS_SERVICE_UUID);
    bleTxChar = nusSvc->createCharacteristic(NUS_CHAR_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    nusSvc->createCharacteristic(NUS_CHAR_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    nusSvc->start();

    // HM-10 serial service — SW Maps "Generic NMEA (Bluetooth LE)" and similar apps
    NimBLEService* hm10Svc = bleServer->createService(HM10_SERVICE_UUID);
    bleHm10Char = hm10Svc->createCharacteristic(HM10_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    hm10Svc->start();

    // Advertise with the NUS service UUID so GPS apps find the device.
    // The 128-bit UUID + full name exceeds the 31-byte ad packet limit, so
    // we put the service UUID in the main packet and the device name in the
    // scan response — both are sent together during active scanning.
    // Main advertising packet: NUS service UUID — GPS apps filter by this UUID.
    // Scan response: device name (sent on active scan request).
    // Keep advertising while connected so the device remains discoverable
    // during testing with multiple apps.
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);   // for NUS-based GPS apps
    adv->addServiceUUID(HM10_SERVICE_UUID);  // for HM-10 / Generic NMEA apps
    adv->enableScanResponse(true);

    NimBLEAdvertisementData scanResp;
    scanResp.setName(deviceName);
    adv->setScanResponseData(scanResp);

    // Continue advertising after a client connects so other apps can
    // still discover the device (they'll connect once the first client drops)
    adv->setMinInterval(0x20);   // 20ms — responsive discovery
    adv->setMaxInterval(0x40);   // 40ms

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

    // Send on NUS TX characteristic
    int offset = 0;
    while (offset < len) {
        int chunk = min((int)mtu, len - offset);
        bleTxChar->setValue((uint8_t*)(buf + offset), chunk);
        bleTxChar->notify();
        offset += chunk;
        if (offset < len) delay(10);
    }

    // Send on HM-10 characteristic (same data, different service)
    if (bleHm10Char) {
        offset = 0;
        while (offset < len) {
            int chunk = min((int)mtu, len - offset);
            bleHm10Char->setValue((uint8_t*)(buf + offset), chunk);
            bleHm10Char->notify();
            offset += chunk;
            if (offset < len) delay(10);
        }
    }
}
