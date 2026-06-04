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
    uart_flush_input(port);
}

static void um982ConfigureNMEA(uart_port_t port) {
    um982Cmd(port, "UNLOGALL COM1");
    um982FlushRx(port, 200);
    um982Cmd(port, "LOG COM1 GNGGA ONTIME 1");
    um982Cmd(port, "LOG COM1 GPHDT ONTIME 1");
    um982Cmd(port, "LOG COM1 HEADINGA ONTIME 1");
    um982Cmd(port, "LOG COM1 GPVTG ONTIME 1");
    um982Cmd(port, "LOG COM1 GNRMC ONTIME 1");
    um982Cmd(port, "SAVECONFIG");
}

void um982Init(uart_port_t port) {
    vTaskDelay(pdMS_TO_TICKS(500));
    um982FlushRx(port, 300);
    um982ConfigureNMEA(port);
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
    ESP_LOGI(UM982_TAG, "Factory reset complete");
}
