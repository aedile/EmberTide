idf_component_register(
    SRCS
        "src/fq_framebuffer.c"
        "src/fq_sprite.c"
        "src/fq_text.c"
        "src/asset_data.c"
        "src/sprite_util.c"
        "src/ui_widgets.c"
        "src/screens/screen_home.c"
        "src/screens/screen_idle.c"
        "src/screens/screen_inventory.c"
        "src/screens/screen_stats.c"
        "src/screens/screen_combat.c"
        "src/screens/screen_training.c"
        "src/screens/screen_onboarding.c"
        "src/screens/screen_battle_result.c"
        "src/screens/screen_rebirth.c"
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
    # declarations and implementation. Removed in fix/comprehensive-audit-remediation.
    #
    # Phase-18: asset_data.c is the SINGLE TU that includes the generated
    # sprite/font headers. No other screen .c file may include those headers
    # directly — all access goes through asset_data.h accessor functions.
    #
    # sprite_util.c: 2x sprite blitter, 3x sprite blitter (Phase-19.5),
    # inverted-text renderer, and header/footer bar helpers shared by all
    # screen renderers.
    #
    # screen_idle.c: idle overlay renderer added in Phase-19.5. Renders a
    # 3x-scaled character sprite with name/level after 30s of inactivity.
    #
    # screen_onboarding.c: first-boot character creation screen added in
    # Phase-19. Renders class sprite, stat bars, and class name for FQ_STATE_ONBOARDING.
)
