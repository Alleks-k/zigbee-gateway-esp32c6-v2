idf_component_register(
    SRCS "gateway_web_placeholder.c"
    INCLUDE_DIRS "include"
    REQUIRES gateway_core gateway_net esp_http_server mdns spiffs
)

