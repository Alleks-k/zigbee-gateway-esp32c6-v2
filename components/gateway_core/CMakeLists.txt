set(gateway_core_srcs
    "gateway_core_placeholder.c"
    "src/config_service.c"
    "src/device_service_rules.c"
    "src/device_service.c"
    "src/device_service_lock_freertos.c"
    "src/device_service_persistence.c")

set(gateway_core_reqs
    gateway_core_storage
    gateway_shared_config
)

set(gateway_core_priv_reqs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_srcs "test/gateway_core_self_tests.c")
    list(APPEND gateway_core_priv_reqs
        gateway_core_zigbee
        unity)
endif()

idf_component_register(
    SRCS
        ${gateway_core_srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        ${gateway_core_reqs}
    PRIV_REQUIRES
        ${gateway_core_priv_reqs}
)
