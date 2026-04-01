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
    # hal_epaper.c uses: spi_master (SPI bus), driver/gpio.h, esp_timer, FreeRTOS.
    # hal_gpio.c uses:   driver/gpio.h, esp_timer, FreeRTOS queues/tasks.
    # hal_flash.c uses:  joltwallet/littlefs (fetched via idf_component.yml).
    #   Component registry name: joltwallet/littlefs -> component: joltwallet__littlefs
    #   which provides the esp_vfs_littlefs.h API.
    # hal_audio.c stub: will need driver (LEDC) + esp_timer in Phase 17.
    # hal_sleep.c stub: will need esp_sleep + driver/rtc_io in Phase 17.
    # hal_ble.c stub:   will need bt (NimBLE) in Phase 17.
    # hal_wifi.c stub:  will need esp_wifi + esp_http_server + nvs_flash in Phase 17.
    REQUIRES
        driver
        esp_timer
    PRIV_REQUIRES
        joltwallet__littlefs
)
