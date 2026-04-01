idf_component_register(
    SRCS
        "src/types.c"
        "src/prng.c"
        "src/crc32.c"
        "src/progression.c"
        "src/save_format.c"
        "src/combat.c"
        "src/item_engine.c"
    INCLUDE_DIRS
        "include"
    # game/ MUST NOT depend on hal/, presentation/, or connectivity/.
    # This component declares no REQUIRES or PRIV_REQUIRES to those layers.
    # Enforced by CMake include path isolation and try_compile boundary checks.
)
