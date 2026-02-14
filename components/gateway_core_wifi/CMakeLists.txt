idf_component_register(
    SRCS
        "src/wifi_service.c"
    INCLUDE_DIRS
        "../gateway_core/include"
    REQUIRES
        gateway_core_storage
)
