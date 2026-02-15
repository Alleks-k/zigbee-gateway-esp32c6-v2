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

extern int ws_fds[MAX_WS_CLIENTS];
extern httpd_handle_t s_server;
extern SemaphoreHandle_t s_ws_mutex;
extern SemaphoreHandle_t s_ws_broadcast_mutex;
extern esp_event_handler_instance_t s_list_changed_handler;
extern esp_event_handler_instance_t s_lqi_changed_handler;
extern esp_timer_handle_t s_ws_debounce_timer;
extern esp_timer_handle_t s_ws_periodic_timer;
extern char s_ws_devices_json_buf[WS_JSON_BUF_SIZE];
extern char s_last_ws_devices_json[WS_JSON_BUF_SIZE];
extern size_t s_last_ws_devices_json_len;
extern int64_t s_last_ws_devices_send_us;
extern char s_ws_health_json_buf[WS_JSON_BUF_SIZE];
extern char s_last_ws_health_json[WS_JSON_BUF_SIZE];
extern size_t s_last_ws_health_json_len;
extern int64_t s_last_ws_health_send_us;
extern char s_ws_lqi_json_buf[WS_JSON_BUF_SIZE];
extern char s_last_ws_lqi_json[WS_JSON_BUF_SIZE];
extern size_t s_last_ws_lqi_json_len;
extern int64_t s_last_ws_lqi_send_us;
extern char s_ws_frame_buf[WS_FRAME_BUF_SIZE];
extern uint32_t s_ws_seq;
extern api_ws_runtime_metrics_t s_ws_metrics;

#if CONFIG_GATEWAY_SELF_TEST_APP
extern ws_manager_transport_ops_t s_ws_transport_ops;
#endif
