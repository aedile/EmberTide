idf_component_register(
    SRCS
        "src/hal_gpio.c"
    INCLUDE_DIRS
        "include"
    # hal/ sits at the bottom of the dependency graph.
    # It REQUIRES ESP-IDF SDK components (added in later phases).
    # It MUST NOT be depended upon by game/, presentation/, or connectivity/.
)
