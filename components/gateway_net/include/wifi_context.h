#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "state_store.h"

struct wifi_runtime_ctx;

#if CONFIG_GATEWAY_SELF_TEST_APP
typedef struct {
    void (*net_platform_services_init)(struct wifi_runtime_ctx *ctx);
    esp_err_t (*wifi_sta_connect_and_wait)(struct wifi_runtime_ctx *ctx);
    esp_err_t (*wifi_start_fallback_ap)(struct wifi_runtime_ctx *ctx);
} wifi_init_ops_t;
#endif

typedef struct wifi_runtime_ctx {
    EventGroupHandle_t wifi_event_group;
    int retry_num;
    char active_ssid[33];
    bool ap_netif_created;
    esp_netif_t *ap_netif;
    bool fallback_ap_active;
    bool sta_connected;
    bool loaded_from_nvs;
    gateway_state_handle_t gateway_state;
    struct wifi_service *wifi_service;
    struct system_service *system_service;
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
#if CONFIG_GATEWAY_SELF_TEST_APP
    wifi_init_ops_t ops;
#endif
} wifi_runtime_ctx_t;
