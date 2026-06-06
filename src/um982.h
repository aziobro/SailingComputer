#pragma once
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

static void um982ConfigureNMEA(uart_port_t port) {
    // Configure the physical port receiving these commands. Omitting a COM
    // argument avoids assuming how a particular UM982 breakout labels TX/RX.
    um982Cmd(port, "UNLOG");
    um982FlushRx(port, 200);
    um982Cmd(port, "GNGGA 1");
    um982Cmd(port, "GPHDT 1");
    um982Cmd(port, "HEADINGA 1");
    um982Cmd(port, "GPVTG 1");
    um982Cmd(port, "GNRMC 1");
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

void um982Init(uart_port_t port) {
    vTaskDelay(pdMS_TO_TICKS(500));
    um982FlushRx(port, 300);
    um982ConfigureNMEA(port);
    um982ConfigureRTK(port);
    ESP_LOGI(UM982_TAG, "Init complete");
}

void um982FactoryReset(uart_port_t port) {
    ESP_LOGI(UM982_TAG, "Factory reset...");
    uart_write_bytes(port, "FRESET\r\n", 8);
    vTaskDelay(pdMS_TO_TICKS(6000));
    um982FlushRx(port, 500);
    um982Cmd(port, "CONFIG SIGNALGROUP 4 5");
    ESP_LOGI(UM982_TAG, "Waiting for SIGNALGROUP reset...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    um982FlushRx(port, 500);
    um982ConfigureNMEA(port);
    um982ConfigureRTK(port);
    ESP_LOGI(UM982_TAG, "Factory reset complete");
}
