set(gateway_web_srcs
    "src/web_server.c"
    "src/api_handlers.c"
    "src/api_contracts.c"
    "src/api_usecases.c"
    "src/http_error.c"
    "src/http_routes.c"
    "src/http_static.c"
    "src/ws_manager.c"
    "src/error_ring.c"
    "src/mdns_service.c"
    "gateway_web_placeholder.c")

set(gateway_web_reqs
    gateway_core
    gateway_net
    esp_http_server
    mdns
    spiffs
    json
    nvs_flash
    esp_wifi
    esp_event
    esp_timer)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_web_srcs "test/gateway_web_self_tests.c")
    list(APPEND gateway_web_reqs unity)
endif()

idf_component_register(
    SRCS
        ${gateway_web_srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        ${gateway_web_reqs}
)
