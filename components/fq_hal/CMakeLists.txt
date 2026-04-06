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
    # hal_audio.c: delegates to codec_board component (factory program port).
    #   codec_board owns the esp_codec_dev + driver/i2c_master + driver/i2s_std deps.
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

# Audio hardware path — DISABLED.
# LoadStoreAlignment crash in i2s_channel_init_std_mode persists regardless of
# SPIRAM config. Root cause: esp_codec_dev managed component (v1.5.7) may have
# ABI mismatch with ESP-IDF 5.3.4. Needs investigation with matching IDF version
# or raw I2S driver without esp_codec_dev.
# target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_BSP_AUDIO=1)
# PROGRESS: I2S handles now in internal SRAM (0x3FCA range) with ALWAYSINTERNAL=16384.
# Still crashes in GDMA interrupt setup — likely uninitialized struct field or
# IDF 5.3 bug in intr_alloc path. Need to decode EXCVADDR=0x400556e5.
