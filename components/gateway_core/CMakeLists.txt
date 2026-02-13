set(gateway_core_srcs
    "src/rcp_tool.c"
    "src/device_manager.c"
    "src/gateway_events.c"
    "src/gateway_state.c"
    "src/settings_manager.c"
    "src/job_queue.c"
    "src/zigbee_service.c"
    "src/wifi_service.c"
    "src/system_service.c"
    "gateway_core_placeholder.c")

set(gateway_core_reqs
    nvs_flash
    json
    esp-zigbee-lib
    esp_rcp_update
    spiffs
    esp_wifi
    esp_event
    esp_timer
    esp_partition
    esp_system)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_core_srcs "test/gateway_core_self_tests.c")
    list(APPEND gateway_core_reqs unity)
endif()

idf_component_register(
    SRCS
        ${gateway_core_srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        ${gateway_core_reqs}
)
