idf_component_register(
    SRCS
        "src/hal_gpio.c"
        "src/hal_epaper.c"
        "src/hal_flash.c"
        "src/hal_audio.c"
        "src/hal_sleep.c"
        "src/hal_ble.c"
        "src/hal_wifi.c"
    INCLUDE_DIRS
        "include"
    # Phase 16: Real ESP-IDF driver implementations added.
    # hal_epaper.c uses: spi_master (SPI bus), driver/gpio.h, esp_timer, FreeRTOS, esp_log.
    # hal_gpio.c uses:   driver/gpio.h, esp_timer, FreeRTOS queues/tasks.
    # hal_flash.c uses:  joltwallet/littlefs (fetched via idf_component.yml).
    #   Component registry name: joltwallet/littlefs -> component: joltwallet__littlefs
    #   which provides the esp_vfs_littlefs.h API.
    # hal_audio.c: Phase 23 — delegates to codec_board component (factory program port).
    #   codec_board owns the esp_codec_dev + driver/i2c_master + driver/i2s_std deps.
    #   Audio hardware path enabled via CONFIG_BSP_AUDIO=1 at bottom of this file.
    #   IDF 5.5.1 required: fixes GDMA interrupt bugs #15533 + 8dfcafa.
    # hal_sleep.c stub: will need esp_sleep + driver/rtc_io in Phase 17.
    # hal_ble.c stub:   will need bt (NimBLE) in Phase 17.
    # hal_wifi.c stub:  will need esp_wifi + esp_http_server + nvs_flash in Phase 17.
    REQUIRES
        driver
        esp_timer
        freertos
        codec_board
    PRIV_REQUIRES
        joltwallet__littlefs
)

# Phase 23: Enable real audio hardware via codec_board + esp_codec_dev.
# Requires IDF 5.5.1+ (fixes GDMA interrupt bugs #15533 and 8dfcafa).
# SPIRAM_MALLOC_ALWAYSINTERNAL=16384 must be set in sdkconfig.defaults so
# I2S channel handles remain in internal SRAM (not PSRAM).
target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_BSP_AUDIO=1)
