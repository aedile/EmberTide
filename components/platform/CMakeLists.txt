idf_component_register(
    SRCS
        "src/storage.c"
    INCLUDE_DIRS
        "include"
    # platform/ sits between game/presentation and hal.
    # It REQUIRES hal/ (added in later phases when ESP-IDF drivers are wired).
    # game/ and presentation/ do NOT depend on platform/ directly.
)
