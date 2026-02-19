#pragma once

#include "ws_manager.h"

#include "api_usecases.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_WS_CLIENTS 8
#define WS_JSON_BUF_SIZE 2048
#define WS_FRAME_BUF_SIZE 2200
#define WS_PROTOCOL_VERSION 1
#define WS_MIN_DUP_BROADCAST_INTERVAL_US (250 * 1000)
#define WS_MIN_BROADCAST_INTERVAL_US (120 * 1000)
#define WS_MIN_HEALTH_BROADCAST_INTERVAL_US (800 * 1000)
#define WS_MIN_LQI_BROADCAST_INTERVAL_US (800 * 1000)
#define WS_BROADCAST_RETRY_US (20 * 1000)

typedef struct ws_manager_ctx {
    int ws_fds[MAX_WS_CLIENTS];
    httpd_handle_t server;
    SemaphoreHandle_t ws_mutex;
    SemaphoreHandle_t ws_broadcast_mutex;
    esp_event_handler_instance_t list_changed_handler;
    esp_event_handler_instance_t lqi_changed_handler;
    esp_timer_handle_t ws_debounce_timer;
    esp_timer_handle_t ws_periodic_timer;
    char ws_devices_json_buf[WS_JSON_BUF_SIZE];
    char last_ws_devices_json[WS_JSON_BUF_SIZE];
    size_t last_ws_devices_json_len;
    int64_t last_ws_devices_send_us;
    char ws_health_json_buf[WS_JSON_BUF_SIZE];
    char last_ws_health_json[WS_JSON_BUF_SIZE];
    size_t last_ws_health_json_len;
    int64_t last_ws_health_send_us;
    char ws_lqi_json_buf[WS_JSON_BUF_SIZE];
    char last_ws_lqi_json[WS_JSON_BUF_SIZE];
    size_t last_ws_lqi_json_len;
    int64_t last_ws_lqi_send_us;
    char ws_frame_buf[WS_FRAME_BUF_SIZE];
    uint32_t ws_seq;
    api_ws_runtime_metrics_t ws_metrics;
#if CONFIG_GATEWAY_SELF_TEST_APP
    ws_manager_transport_ops_t ws_transport_ops;
#endif
} ws_manager_ctx_t;
