idf_component_register(
    SRCS
        "src/hal_gpio.c"
        "src/hal_epaper.c"
        "src/hal_flash.c"
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
)
