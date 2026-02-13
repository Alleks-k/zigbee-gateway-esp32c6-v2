#pragma once

#include <stdbool.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"

typedef struct {
    EventGroupHandle_t wifi_event_group;
    int retry_num;
    char active_ssid[33];
    bool ap_netif_created;
    esp_netif_t *ap_netif;
    bool fallback_ap_active;
    bool sta_connected;
    bool loaded_from_nvs;
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
} wifi_runtime_ctx_t;
