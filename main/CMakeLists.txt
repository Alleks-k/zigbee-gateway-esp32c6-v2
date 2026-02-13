set(main_srcs
    "core/rcp_tool.c"
    "core/device_manager.c"
    "core/gateway_events.c"
    "core/settings_manager.c"
    "core/zigbee_service.c"
    "core/wifi_service.c"
    "core/system_service.c"
    "net/wifi_init.c"
    "net/wifi_sta.c"
    "net/wifi_ap_fallback.c"
    "web/web_server.c"
    "web/api_handlers.c"
    "web/http_routes.c"
    "web/http_static.c"
    "web/ws_manager.c"
    "web/mdns_service.c"
)

if(CONFIG_ZGW_SELF_TEST_APP)
    list(APPEND main_srcs "core/self_tests.c" "core/self_tests_app_main.c")
else()
    list(APPEND main_srcs "core/esp_zigbee_gateway.c")
endif()

# Реєстрація компонентів вашого проєкту
idf_component_register(SRCS ${main_srcs}
                       INCLUDE_DIRS "." "core" "net" "web"
                       REQUIRES mdns esp-zigbee-lib nvs_flash esp_http_server json spiffs unity
                       PRIV_REQUIRES esp_wifi esp_timer)

# Генерація SPIFFS іміджу для папки www
spiffs_create_partition_image(www web/www FLASH_IN_PROJECT)

# Блок оновлення RCP (залишаємо як було у вас)
if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    idf_build_get_property(python PYTHON)
    add_custom_target(rcp_image_generation ALL
    COMMAND ${python} ${CMAKE_CURRENT_SOURCE_DIR}/../create_ota_image.py
    --rcp-build-dir ${IDF_PATH}/examples/openthread/ot_rcp/build
    --target-file ${CMAKE_CURRENT_BINARY_DIR}/spiffs_image/ot_rcp_0/rcp_image
    )
    spiffs_create_partition_image(rcp_fw ${CMAKE_CURRENT_BINARY_DIR}/spiffs_image FLASH_IN_PROJECT
                              DEPENDS rcp_image_generation)
endif()
