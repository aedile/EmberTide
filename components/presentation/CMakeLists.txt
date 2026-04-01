idf_component_register(
    SRCS
        "src/renderer.c"
    INCLUDE_DIRS
        "include"
    # presentation/ depends on game/ for view model construction helpers
    # (added in later phases). Does NOT depend on hal/ — enforced here.
    REQUIRES
        game
    # screen_mgr.c (device-only) is excluded from this SRCS list until
    # it is implemented; it will live in a separate device-only build target.
)
