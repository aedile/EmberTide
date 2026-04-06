idf_component_register(
    SRCS
        "app_main.c"
        "vm_builder.c"
        "event_bus.c"
        "app_fsm.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        game
        presentation
        platform
        connectivity
        fq_hal
    # Phase 16: fq_hal added to REQUIRES so app_main.c can call
    # hal_epaper_init/flush, hal_gpio_init, hal_flash_init/read/write directly.
    # Architecture note: main/ is the ONLY module permitted to span all layers.
    # game/, presentation/, and connectivity/ still MUST NOT depend on fq_hal.
    EMBED_FILES
        # Phase 23: Embedded MOD music for in-game audio playback.
        # "8-bit Castle.mod" by Ozzed (CC0 public domain), 57KB.
        # Accessed via: extern const uint8_t test_music_mod_start[];
        #               extern const uint8_t test_music_mod_end[];
        "test_music.mod"
)
