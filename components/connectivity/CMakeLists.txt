idf_component_register(
    SRCS
        "src/protocol.c"
        "src/sync.c"
    INCLUDE_DIRS
        "include"
    # PRIV_REQUIRES game: protocol.c uses fq_crc32() from components/game/src/crc32.c.
    # This is a PRIVATE dependency — crc32.h is NOT re-exported via connectivity/'s
    # public include path. Consumers of connectivity/ cannot transitively pull in
    # game/ headers through this dependency.
    #
    # Architecture note: The N1c boundary check (bound_n1c_connectivity_includes_game.c)
    # verifies that the PUBLIC include path of connectivity/ is isolated from game/.
    # PRIV_REQUIRES does not violate this boundary — it only affects the compilation
    # unit of protocol.c itself.
    #
    # Future option: move crc32 to a standalone utils/ component to eliminate this
    # dependency entirely. Track as ADV-P10-01 (DEFERRED).
    #
    # D-03: wifi_service.c and wifi_service.h were empty placeholders. The real
    # WiFi HAL implementation lives in components/fq_hal/src/hal_wifi.c.
    # Removed in fix/comprehensive-audit-remediation.
    PRIV_REQUIRES
        game
)
