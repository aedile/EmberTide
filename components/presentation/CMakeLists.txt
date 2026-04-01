idf_component_register(
    SRCS
        "src/renderer.c"
    INCLUDE_DIRS
        "include"
    # presentation/ does NOT depend on game/ — view model construction happens
    # in main/ (view_model_builder.c), never inside this component.
    # Architecture rule: presentation/ must not import game/ headers directly.
    # Enforced by boundary test N1b (bound_n1b_game_includes_presentation.c).
    #
    # screen_mgr.c (device-only, calls hal_epaper SPI) is excluded from this
    # SRCS list until implementation; it will live in a separate device-only
    # build target that may add REQUIRES hal as needed.
)
