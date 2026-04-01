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
    # hal/ is NOT required directly by main — it is accessed through platform/.
    # This enforces the layered dependency: main -> platform -> hal.
)
