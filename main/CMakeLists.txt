set(main_srcs
)

set(main_reqs
    gateway_app
)

set(main_priv_reqs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND main_srcs
        "tests/self_tests.c"
        "tests/e2e_self_tests.c"
        "tests/self_tests_app_main.c")
    list(APPEND main_reqs
        gateway_core_state
        gateway_core_wifi
        gateway_core_jobs
        unity)
    list(APPEND main_priv_reqs
        esp_wifi
        esp_timer)
else()
    list(APPEND main_srcs "main.c")
endif()

# Реєстрація компонентів вашого проєкту
idf_component_register(SRCS ${main_srcs}
                       INCLUDE_DIRS "." "tests"
                       REQUIRES ${main_reqs}
                       PRIV_REQUIRES ${main_priv_reqs})

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
