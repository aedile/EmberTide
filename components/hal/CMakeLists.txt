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
    # hal/ sits at the bottom of the dependency graph.
    # It REQUIRES ESP-IDF SDK components (added in later phases).
    # It MUST NOT be depended upon by game/, presentation/, or connectivity/.
    #
    # Phase 12 note: hal_epaper.c and hal_flash.c are stubs that compile
    # without ESP-IDF driver headers.  When real SPI and LittleFS
    # implementations are added, add the following to REQUIRES:
    #   driver esp_vfs_littlefs
    #
    # Phase 13 note: hal_audio.c and hal_sleep.c are stubs that compile
    # without ESP-IDF driver headers.  When LEDC and deep-sleep
    # implementations are added, add to REQUIRES:
    #   driver esp_timer
    #
    # Phase 14 note: hal_ble.c and hal_wifi.c are stubs that compile
    # without NimBLE or ESP-IDF WiFi/HTTPD headers.  When real NimBLE and
    # WiFi driver implementations are added, add to REQUIRES:
    #   bt esp_wifi esp_http_server nvs_flash
)
