idf_component_register(
    SRCS
        "src/wifi_service.c"
    INCLUDE_DIRS
        "include"
    # connectivity/ MUST NOT depend on game/.
    # It MAY depend on platform/ for network storage (added in later phases).
    # It REQUIRES ESP-IDF networking components (added in later phases).
)
