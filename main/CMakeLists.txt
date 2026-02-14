set(main_srcs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND main_srcs
        "tests/self_tests.c"
        "tests/e2e_self_tests.c"
        "tests/self_tests_app_main.c")
else()
    list(APPEND main_srcs "core/esp_zigbee_gateway.c")
endif()

# Реєстрація компонентів вашого проєкту
idf_component_register(SRCS ${main_srcs}
                       INCLUDE_DIRS "." "core" "tests"
                       REQUIRES mdns esp-zigbee-lib nvs_flash esp_http_server json spiffs unity gateway_core gateway_core_events gateway_core_storage gateway_core_system gateway_core_zigbee gateway_net gateway_web
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
