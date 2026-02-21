set(gateway_core_srcs
    "gateway_core_placeholder.c"
    "src/config_service.c"
    "src/device_service_rules.c"
    "src/device_service.c"
    "src/device_service_persistence.c")

set(gateway_core_include_dirs
    "include")

set(gateway_core_priv_include_dirs
)

set(gateway_core_reqs
    gateway_core_persistence_adapter
    gateway_shared_config
)

set(gateway_core_priv_reqs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_srcs "test/gateway_core_self_tests.c")
    # Ensure component-local self-tests can include unity.h in self-test builds.
    list(APPEND gateway_core_reqs
        unity)
    list(APPEND gateway_core_priv_reqs
        gateway_core_zigbee
        unity)
    list(APPEND gateway_core_priv_include_dirs
        "${CMAKE_SOURCE_DIR}/components/gateway_core_zigbee/include"
        "${IDF_PATH}/components/unity/include"
        "${IDF_PATH}/components/unity/unity/src")
endif()

idf_component_register(
    SRCS
        ${gateway_core_srcs}
    INCLUDE_DIRS
        ${gateway_core_include_dirs}
    PRIV_INCLUDE_DIRS
        ${gateway_core_priv_include_dirs}
    REQUIRES
        ${gateway_core_reqs}
    PRIV_REQUIRES
        ${gateway_core_priv_reqs}
)
