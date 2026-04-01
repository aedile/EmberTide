idf_component_register(
    SRCS
        "src/fq_framebuffer.c"
        "src/fq_sprite.c"
        "src/fq_text.c"
        "src/ui_widgets.c"
        "src/screens/screen_home.c"
        "src/screens/screen_inventory.c"
        "src/screens/screen_stats.c"
        "src/screens/screen_combat.c"
        "src/screens/screen_training.c"
    INCLUDE_DIRS
        "include"
    # presentation/ does NOT depend on game/ — view model construction happens
    # in main/ (vm_builder.c), never inside this component.
    # Architecture rule: presentation/ must not import game/ headers directly.
    # Enforced by boundary test N1b (bound_n1b_game_includes_presentation.c).
    #
    # screen_mgr.c (device-only, calls hal_epaper SPI) is excluded from this
    # SRCS list until implementation; it will live in a separate device-only
    # build target that may add REQUIRES fq_hal as needed.
    #
    # D-01: renderer.c and renderer.h were empty placeholders with no
    # declarations or implementation. Removed in fix/comprehensive-audit-remediation.
)
