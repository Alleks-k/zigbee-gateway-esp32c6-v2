set(gateway_core_storage_srcs
    "src/config_repository_nvs.c"
    "src/device_repository_nvs.c"
    "src/storage_kv_nvs.c"
    "src/storage_partitions_nvs.c"
    "src/storage_schema_nvs.c"
)

set(gateway_core_storage_include_dirs
    "include")

set(gateway_core_storage_priv_include_dirs
)

set(gateway_core_storage_reqs
    gateway_shared_config)

set(gateway_core_storage_priv_reqs
    nvs_flash
    esp_partition)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_storage_srcs "test/gateway_core_storage_self_tests.c")
    # For self-test component-local test sources, export unity headers explicitly.
    list(APPEND gateway_core_storage_reqs unity)
    list(APPEND gateway_core_storage_priv_reqs unity)
    list(APPEND gateway_core_storage_priv_include_dirs
        "${IDF_PATH}/components/unity/include"
        "${IDF_PATH}/components/unity/unity/src")
endif()

idf_component_register(
    SRCS
        ${gateway_core_storage_srcs}
    INCLUDE_DIRS
        ${gateway_core_storage_include_dirs}
    PRIV_INCLUDE_DIRS
        ${gateway_core_storage_priv_include_dirs}
    REQUIRES
        ${gateway_core_storage_reqs}
    PRIV_REQUIRES
        ${gateway_core_storage_priv_reqs}
)
