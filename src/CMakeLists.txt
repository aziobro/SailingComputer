FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.cpp)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES
        mdns
        esp_wifi
        esp_http_server
        esp_https_server
        esp_hosted
        esp_app_format
        bootloader_support
        app_update
        esp_netif
        nvs_flash
        driver
        bt
        lwip
        mbedtls
        spiffs
        fatfs
        esp_driver_sdmmc
        sdmmc
        json
    EMBED_FILES
        # ESP32-C6 esp_hosted slave firmware. Refresh from the GPS project or
        # a compatible network_adapter build when upgrading the coprocessor.
        "c6_slave_fw.bin"
)
