#pragma once
#include <Arduino.h>

// Send a command to the UM982, wait briefly, then echo the response to debug serial.
static void um982Cmd(HardwareSerial &ser, const char *cmd) {
    ser.println(cmd);
    Serial.printf("[UM982] > %s\n", cmd);
    delay(200);
    uint32_t t = millis();
    while (millis() - t < 100) {
        while (ser.available()) {
            char c = (char)ser.read();
            if (c >= 0x20 || c == '\n') Serial.write(c);
        }
    }
}

// Drain the UM982 RX buffer for `ms` milliseconds, discarding all incoming bytes.
// Used after resets to discard binary boot output before sending configuration commands.
static void um982FlushRx(HardwareSerial &ser, uint32_t ms = 500) {
    uint32_t t = millis();
    while (millis() - t < ms) {
        while (ser.available()) ser.read();
        delay(10);
    }
}

// Configure the 5 NMEA output sentences on COM1. Called by both init and factory reset.
static void um982ConfigureNMEA(HardwareSerial &ser) {
    um982Cmd(ser, "UNLOGALL COM1");
    um982FlushRx(ser, 200);
    um982Cmd(ser, "LOG COM1 GNGGA ONTIME 1");    // position + fix quality
    um982Cmd(ser, "LOG COM1 GPHDT ONTIME 1");    // true heading (dual-antenna)
    um982Cmd(ser, "LOG COM1 HEADINGA ONTIME 1"); // Unicore proprietary: heading + roll
    um982Cmd(ser, "LOG COM1 GPVTG ONTIME 1");    // course and speed over ground
    um982Cmd(ser, "LOG COM1 GNRMC ONTIME 1");    // date/time + basic nav
    um982Cmd(ser, "SAVECONFIG");
}

// Normal boot init — restores NMEA output config only.
// SIGNALGROUP 4 5 is already saved in UM982 flash so does not need to be resent.
// Call um982FactoryReset() via the web UI System tab only when hardware changes require it.
void um982Init(HardwareSerial &ser) {
    delay(500);
    um982FlushRx(ser, 300);
    um982ConfigureNMEA(ser);
    Serial.println("[UM982] Init complete");
}

// Full reconfiguration — wipes flash and rebuilds from scratch.
// Call once when antennas are first installed or if the UM982 config gets corrupted.
// Takes ~15 seconds due to SIGNALGROUP reset.
void um982FactoryReset(HardwareSerial &ser) {
    Serial.println("[UM982] Factory reset...");
    ser.println("FRESET");
    delay(6000);
    um982FlushRx(ser, 500);

    // Dual-antenna signal groups: complementary pair required for heading + roll output.
    // Group 4: GPS+GLONASS+BeiDou  (ANT1 = primary/starboard)
    // Group 5: GPS+Galileo+BeiDou  (ANT2 = heading/port)
    um982Cmd(ser, "CONFIG SIGNALGROUP 4 5");
    Serial.println("[UM982] Waiting for SIGNALGROUP reset...");
    delay(5000);
    um982FlushRx(ser, 500);

    um982ConfigureNMEA(ser);
    Serial.println("[UM982] Factory reset complete");
}
