idf_component_register(
    SRCS
        "src/gateway_events.c"
    INCLUDE_DIRS
        "../gateway_core/include"
    REQUIRES
        esp_event
)
