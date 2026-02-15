set(gateway_core_storage_srcs
    "src/device_repository_nvs.c"
    "src/gateway_state.c"
    "src/storage_kv_nvs.c"
    "src/settings_manager.c")

set(gateway_core_storage_reqs
    gateway_core_events
    gateway_shared_config)

set(gateway_core_storage_priv_reqs
    nvs_flash
    esp_event
    esp_timer
    esp_partition)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_storage_srcs "test/gateway_core_storage_self_tests.c")
    list(APPEND gateway_core_storage_priv_reqs unity)
endif()

idf_component_register(
    SRCS
        ${gateway_core_storage_srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        ${gateway_core_storage_reqs}
    PRIV_REQUIRES
        ${gateway_core_storage_priv_reqs}
)
