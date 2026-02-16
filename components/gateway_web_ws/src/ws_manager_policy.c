#include "ws_manager.h"

#include "health_json_builder.h"
#include "lqi_json_mapper.h"
#include "status_json_builder.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ws_manager_internal.h"
#include "ws_manager_json.h"
#include "ws_manager_state.h"
#include "ws_manager_transport.h"

#include <string.h>

static const char *TAG = "WS_POLICY";

void ws_broadcast_status(void)
{
    if (!s_server) {
        return;
    }
    if (s_ws_broadcast_mutex && xSemaphoreTake(s_ws_broadcast_mutex, 0) != pdTRUE) {
        ws_manager_inc_lock_skips();
        if (s_ws_debounce_timer) {
            (void)esp_timer_stop(s_ws_debounce_timer);
            (void)esp_timer_start_once(s_ws_debounce_timer, WS_BROADCAST_RETRY_US);
        }
        return;
    }

    bool release_broadcast_lock = (s_ws_broadcast_mutex != NULL);
    size_t json_len = 0;
    esp_err_t build_ret = build_devices_json_compact(s_ws_devices_json_buf, sizeof(s_ws_devices_json_buf), &json_len);
    if (build_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to build WS delta JSON payload: %s", esp_err_to_name(build_ret));
        goto out;
    }

    int64_t now_us = esp_timer_get_time();
    bool same_payload = (json_len == s_last_ws_devices_json_len) &&
                        (json_len > 0) &&
                        (memcmp(s_ws_devices_json_buf, s_last_ws_devices_json, json_len) == 0);
    if (same_payload && (now_us - s_last_ws_devices_send_us) < WS_MIN_DUP_BROADCAST_INTERVAL_US) {
        goto out;
    }

    int64_t elapsed_us = now_us - s_last_ws_devices_send_us;
    if (s_last_ws_devices_send_us > 0 && elapsed_us < WS_MIN_BROADCAST_INTERVAL_US) {
        if (s_ws_debounce_timer) {
            int64_t delay_us = WS_MIN_BROADCAST_INTERVAL_US - elapsed_us;
            if (delay_us < 1000) {
                delay_us = 1000;
            }
            (void)esp_timer_stop(s_ws_debounce_timer);
            (void)esp_timer_start_once(s_ws_debounce_timer, (uint64_t)delay_us);
        }
        goto out;
    }

    size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t frame_len = 0;
    esp_err_t wrap_ret = ws_manager_wrap_event_payload("devices_delta", s_ws_devices_json_buf, json_len, &frame_len);
    if (wrap_ret == ESP_OK) {
        (void)ws_manager_send_frame_to_clients(s_ws_frame_buf, frame_len);
    } else {
        ESP_LOGW(TAG, "Failed to wrap WS devices frame: %s", esp_err_to_name(wrap_ret));
    }

    if (json_len < sizeof(s_last_ws_devices_json)) {
        memcpy(s_last_ws_devices_json, s_ws_devices_json_buf, json_len);
        s_last_ws_devices_json[json_len] = '\0';
        s_last_ws_devices_json_len = json_len;
    } else {
        s_last_ws_devices_json_len = 0;
    }
    s_last_ws_devices_send_us = now_us;

    if ((now_us - s_last_ws_health_send_us) >= WS_MIN_HEALTH_BROADCAST_INTERVAL_US) {
        size_t health_len = 0;
        esp_err_t health_ret = build_health_json_compact(s_ws_health_json_buf, sizeof(s_ws_health_json_buf), &health_len);
        if (health_ret == ESP_OK) {
            bool same_health = (health_len == s_last_ws_health_json_len) &&
                               (health_len > 0) &&
                               (memcmp(s_ws_health_json_buf, s_last_ws_health_json, health_len) == 0);
            if (!same_health || (now_us - s_last_ws_health_send_us) >= WS_MIN_DUP_BROADCAST_INTERVAL_US) {
                wrap_ret = ws_manager_wrap_event_payload("health_state", s_ws_health_json_buf, health_len, &frame_len);
                if (wrap_ret == ESP_OK) {
                    (void)ws_manager_send_frame_to_clients(s_ws_frame_buf, frame_len);
                    if (health_len < sizeof(s_last_ws_health_json)) {
                        memcpy(s_last_ws_health_json, s_ws_health_json_buf, health_len);
                        s_last_ws_health_json[health_len] = '\0';
                        s_last_ws_health_json_len = health_len;
                    } else {
                        s_last_ws_health_json_len = 0;
                    }
                    s_last_ws_health_send_us = now_us;
                }
            }
        }
    }

    if ((now_us - s_last_ws_lqi_send_us) >= WS_MIN_LQI_BROADCAST_INTERVAL_US) {
        size_t lqi_len = 0;
        esp_err_t lqi_ret = build_lqi_json_compact(s_ws_lqi_json_buf, sizeof(s_ws_lqi_json_buf), &lqi_len);
        if (lqi_ret == ESP_OK) {
            bool same_lqi = (lqi_len == s_last_ws_lqi_json_len) &&
                            (lqi_len > 0) &&
                            (memcmp(s_ws_lqi_json_buf, s_last_ws_lqi_json, lqi_len) == 0);
            if (!same_lqi || (now_us - s_last_ws_lqi_send_us) >= WS_MIN_DUP_BROADCAST_INTERVAL_US) {
                wrap_ret = ws_manager_wrap_event_payload("lqi_update", s_ws_lqi_json_buf, lqi_len, &frame_len);
                if (wrap_ret == ESP_OK) {
                    (void)ws_manager_send_frame_to_clients(s_ws_frame_buf, frame_len);
                    if (lqi_len < sizeof(s_last_ws_lqi_json)) {
                        memcpy(s_last_ws_lqi_json, s_ws_lqi_json_buf, lqi_len);
                        s_last_ws_lqi_json[lqi_len] = '\0';
                        s_last_ws_lqi_json_len = lqi_len;
                    } else {
                        s_last_ws_lqi_json_len = 0;
                    }
                    s_last_ws_lqi_send_us = now_us;
                }
            }
        }
    }

    size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGD(TAG, "WS broadcast heap: before=%u after=%u delta=%d",
             (unsigned)heap_before, (unsigned)heap_after, (int)(heap_after - heap_before));
out:
    if (release_broadcast_lock) {
        xSemaphoreGive(s_ws_broadcast_mutex);
    }
}
