idf_component_register(
    SRCS
        "src/device_service_lock_freertos_port.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        gateway_core
)
