#pragma once
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

#define UM982_TAG "UM982"

static void um982Cmd(uart_port_t port, const char *cmd) {
    uart_write_bytes(port, cmd, strlen(cmd));
    uart_write_bytes(port, "\r\n", 2);
    ESP_LOGI(UM982_TAG, "> %s", cmd);
    vTaskDelay(pdMS_TO_TICKS(200));
    uint8_t buf[256];
    int len = uart_read_bytes(port, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
    if (len > 0) {
        buf[len] = '\0';
        for (int i = 0; i < len; i++)
            if (buf[i] < 0x20 && buf[i] != '\n' && buf[i] != '\r') buf[i] = '.';
        ESP_LOGI(UM982_TAG, "< %s", (char *)buf);
    }
}

static void um982FlushRx(uart_port_t port, uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    // Drain by reading rather than uart_flush_input() — flush resets the hardware
    // FIFO and temporarily masks the RX interrupt; on IDF 5.3.x / chip rev 3.1
    // the interrupt sometimes isn't re-enabled, permanently killing RX.
    uint8_t dummy[64];
    while (uart_read_bytes(port, dummy, sizeof(dummy), pdMS_TO_TICKS(5)) > 0) {}
}

// Reconfigure the ESP32 UART baud rate without reinstalling the driver.
static void um982SetEspBaud(uart_port_t port, int baud) {
    uart_set_baudrate(port, baud);
    ESP_LOGI(UM982_TAG, "ESP UART%d baud → %d", port, baud);
}

// Probe for a UM982 response at the current UART baud rate.
// Returns true only if the response is valid ASCII (>60% printable bytes),
// which rules out framing garbage produced when the baud rate is wrong.
static bool um982Probe(uart_port_t port) {
    um982FlushRx(port, 50);
    uart_write_bytes(port, "VERSION\r\n", 9);
    vTaskDelay(pdMS_TO_TICKS(300));
    uint8_t buf[384];
    int len = uart_read_bytes(port, buf, sizeof(buf) - 1, pdMS_TO_TICKS(200));
    if (len <= 0) return false;
    buf[len] = '\0';

    // Count printable ASCII bytes to distinguish valid text from baud-mismatch garbage
    int printable = 0;
    for (int i = 0; i < len; i++)
        if ((buf[i] >= 0x20 && buf[i] <= 0x7E) || buf[i] == '\r' || buf[i] == '\n')
            printable++;
    if (printable * 100 / len < 60) {
        ESP_LOGW(UM982_TAG, "Probe: %d bytes but only %d%% printable — baud mismatch",
                 len, printable * 100 / len);
        return false;
    }

    for (int i = 0; i < len; i++)
        if (buf[i] < 0x20 && buf[i] != '\n' && buf[i] != '\r') buf[i] = '.';
    ESP_LOGI(UM982_TAG, "Probe OK: %s", (char *)buf);
    return true;
}

// Negotiate the UM982 up to NMEA_BAUD.
// Tries NMEA_BAUD first (already configured from a previous boot).
// Falls back to NMEA_BAUD_DEFAULT (post-FRESET / first flash) and negotiates up.
// Returns the baud rate that is now active on both sides.
static int um982NegotiateBaud(uart_port_t port) {
    // Try the target baud first — covers normal reboots
    um982SetEspBaud(port, NMEA_BAUD);
    if (um982Probe(port)) {
        ESP_LOGI(UM982_TAG, "Already at %d baud", NMEA_BAUD);
        return NMEA_BAUD;
    }

    // Fall back to the UM982 power-on default
    ESP_LOGW(UM982_TAG, "No response at %d — trying %d (post-reset default)",
             NMEA_BAUD, NMEA_BAUD_DEFAULT);
    um982SetEspBaud(port, NMEA_BAUD_DEFAULT);
    if (!um982Probe(port)) {
        ESP_LOGE(UM982_TAG, "No response at %d either — UART wiring issue?",
                 NMEA_BAUD_DEFAULT);
        return NMEA_BAUD_DEFAULT;   // best we can do; init will proceed and likely fail gracefully
    }

    // Tell the UM982 to switch its port baud, then immediately follow on the ESP side.
    // The command must be sent at the current (old) baud; the response arrives at the new baud.
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "CONFIG COM2 %d", NMEA_BAUD);
    uart_write_bytes(port, cmd, strlen(cmd));
    uart_write_bytes(port, "\r\n", 2);
    ESP_LOGI(UM982_TAG, "> %s", cmd);
    vTaskDelay(pdMS_TO_TICKS(100));   // give UM982 time to apply before we switch

    um982SetEspBaud(port, NMEA_BAUD);
    um982FlushRx(port, 100);

    // Verify the switch worked
    if (um982Probe(port)) {
        ESP_LOGI(UM982_TAG, "Negotiated to %d baud", NMEA_BAUD);
    } else {
        ESP_LOGE(UM982_TAG, "Baud switch verification failed — continuing anyway");
    }
    return NMEA_BAUD;
}

static void um982ConfigureNMEA(uart_port_t port, uint8_t rateHz) {
    // Clamp to nearest supported rate: 1, 2, 5, 10, 20
    const uint8_t validRates[] = {1, 2, 5, 10, 20};
    uint8_t rate = 1;
    for (uint8_t i = 0; i < sizeof(validRates); i++) {
        if (rateHz >= validRates[i]) rate = validRates[i];
    }

    // UM982 command argument is period in seconds, not frequency.
    // Map Hz → period string: 1→"1", 2→"0.5", 5→"0.2", 10→"0.1", 20→"0.05"
    const char *periods[] = {"1", "0.5", "0.2", "0.1", "0.05"};
    const uint8_t rateMap[] = {1, 2, 5, 10, 20};
    const char *period = "1";
    for (uint8_t i = 0; i < sizeof(rateMap); i++) {
        if (rate == rateMap[i]) { period = periods[i]; break; }
    }

    char cmd[32];
    um982Cmd(port, "UNLOG");
    um982FlushRx(port, 200);

    snprintf(cmd, sizeof(cmd), "GNGGA %s",    period); um982Cmd(port, cmd);
    snprintf(cmd, sizeof(cmd), "GPHDT %s",    period); um982Cmd(port, cmd);
    snprintf(cmd, sizeof(cmd), "HEADINGA %s", period); um982Cmd(port, cmd);
    snprintf(cmd, sizeof(cmd), "GPVTG %s",    period); um982Cmd(port, cmd);
    snprintf(cmd, sizeof(cmd), "GNRMC %s",    period); um982Cmd(port, cmd);

    ESP_LOGI(UM982_TAG, "NMEA configured at %d Hz (period %s s)", rate, period);
}

static void um982ConfigureRTK(uart_port_t port) {
    // Keep the auxiliary COM1 link at the diagnostic baud rate and force the
    // receiver into rover mode. Corrections are injected on proven COM2.
    um982Cmd(port, "CONFIG COM1 115200");
    um982Cmd(port, "MODE ROVER");
    um982FlushRx(port, 200);
    um982Cmd(port, "SAVECONFIG");
}

static bool um982ProbePort(uart_port_t port) {
    uart_write_bytes(port, "VERSION\r\n", 9);
    vTaskDelay(pdMS_TO_TICKS(300));
    uint8_t buf[384];
    int len = uart_read_bytes(port, buf, sizeof(buf) - 1, pdMS_TO_TICKS(200));
    if (len <= 0) {
        ESP_LOGW(UM982_TAG, "No response on auxiliary UM982 UART");
        return false;
    }
    buf[len] = '\0';
    for (int i = 0; i < len; i++)
        if (buf[i] < 0x20 && buf[i] != '\n' && buf[i] != '\r') buf[i] = '.';
    ESP_LOGI(UM982_TAG, "Auxiliary UART probe OK: %s", (char *)buf);
    return true;
}

void um982Init(uart_port_t port, uint8_t rateHz) {
    vTaskDelay(pdMS_TO_TICKS(500));
    um982NegotiateBaud(port);
    um982ConfigureNMEA(port, rateHz);
    um982ConfigureRTK(port);
    ESP_LOGI(UM982_TAG, "Init complete");
}

void um982FactoryReset(uart_port_t port, uint8_t rateHz) {
    ESP_LOGI(UM982_TAG, "Factory reset...");
    uart_write_bytes(port, "FRESET\r\n", 8);
    // FRESET resets baud to 115200 — wait for boot then renegotiate
    vTaskDelay(pdMS_TO_TICKS(6000));
    um982NegotiateBaud(port);
    um982FlushRx(port, 500);
    um982Cmd(port, "CONFIG SIGNALGROUP 4 5");
    ESP_LOGI(UM982_TAG, "Waiting for SIGNALGROUP reset...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    um982FlushRx(port, 500);
    um982ConfigureNMEA(port, rateHz);
    um982ConfigureRTK(port);
    ESP_LOGI(UM982_TAG, "Factory reset complete");
}
