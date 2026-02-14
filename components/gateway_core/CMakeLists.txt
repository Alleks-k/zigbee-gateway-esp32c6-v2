set(gateway_core_srcs
    "gateway_core_placeholder.c")

set(gateway_core_reqs
    gateway_core_events
    gateway_core_storage
    gateway_core_zigbee
    gateway_core_wifi
    gateway_core_system
    gateway_core_jobs
)

set(gateway_core_priv_reqs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_srcs "test/gateway_core_self_tests.c")
    list(APPEND gateway_core_priv_reqs unity)
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
