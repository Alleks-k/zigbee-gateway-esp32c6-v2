#include "ws_manager.h"
#include "api_handlers.h"
#include "gateway_events.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "WS_MANAGER";

#define MAX_WS_CLIENTS 8
#define WS_JSON_BUF_SIZE 2048
#define WS_MIN_DUP_BROADCAST_INTERVAL_US (250 * 1000)
#define WS_MIN_BROADCAST_INTERVAL_US (120 * 1000)

static int ws_fds[MAX_WS_CLIENTS];
static httpd_handle_t s_server = NULL;
static SemaphoreHandle_t s_ws_mutex = NULL;
static esp_event_handler_instance_t s_list_changed_handler = NULL;
static esp_timer_handle_t s_ws_debounce_timer = NULL;
static char s_ws_json_buf[WS_JSON_BUF_SIZE];
static char s_last_ws_json[WS_JSON_BUF_SIZE];
static size_t s_last_ws_json_len = 0;
static int64_t s_last_ws_send_us = 0;

static void ws_debounce_timer_cb(void *arg)
{
    (void)arg;
    ws_broadcast_status();
}

static void ws_remove_fd(int fd)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] == fd) {
            ws_fds[i] = -1;
            break;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
}

static void device_list_changed_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == GATEWAY_EVENT && event_id == GATEWAY_EVENT_DEVICE_LIST_CHANGED) {
        ws_broadcast_status();
    }
}

void ws_manager_init(httpd_handle_t server)
{
    s_server = server;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        ws_fds[i] = -1;
    }

    if (!s_ws_mutex) {
        s_ws_mutex = xSemaphoreCreateMutex();
        if (!s_ws_mutex) {
            ESP_LOGE(TAG, "Failed to create WS mutex");
        }
    }

    if (s_list_changed_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, device_list_changed_handler, NULL, &s_list_changed_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_LIST_CHANGED handler: %s", esp_err_to_name(ret));
        }
    }

    if (s_ws_debounce_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = ws_debounce_timer_cb,
            .arg = NULL,
            .name = "ws_debounce"
        };
        esp_err_t ret = esp_timer_create(&timer_args, &s_ws_debounce_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create WS debounce timer: %s", esp_err_to_name(ret));
        }
    }

    s_last_ws_json_len = 0;
    s_last_ws_send_us = 0;
}

void ws_httpd_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    ws_remove_fd(sockfd);
    close(sockfd);
}

esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WS connection");
        int fd = httpd_req_to_sockfd(req);
        bool added = false;
        if (s_ws_mutex) {
            xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        }
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_fds[i] == fd) {
                added = true;
                break;
            }
        }
        for (int i = 0; i < MAX_WS_CLIENTS && !added; i++) {
            if (ws_fds[i] == -1) {
                ws_fds[i] = fd;
                added = true;
                break;
            }
        }
        if (s_ws_mutex) {
            xSemaphoreGive(s_ws_mutex);
        }
        if (!added) {
            ESP_LOGW(TAG, "WS client rejected: max clients reached (%d)", MAX_WS_CLIENTS);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_send(req, "WS clients limit reached", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        ws_remove_fd(fd);
    }
    return ESP_OK;
}

void ws_broadcast_status(void)
{
    if (!s_server) {
        return;
    }

    size_t json_len = 0;
    esp_err_t build_ret = build_devices_json_compact(s_ws_json_buf, sizeof(s_ws_json_buf), &json_len);
    if (build_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to build WS delta JSON payload: %s", esp_err_to_name(build_ret));
        return;
    }

    int64_t now_us = esp_timer_get_time();
    bool same_payload = (json_len == s_last_ws_json_len) &&
                        (json_len > 0) &&
                        (memcmp(s_ws_json_buf, s_last_ws_json, json_len) == 0);
    if (same_payload && (now_us - s_last_ws_send_us) < WS_MIN_DUP_BROADCAST_INTERVAL_US) {
        return;
    }

    int64_t elapsed_us = now_us - s_last_ws_send_us;
    if (s_last_ws_send_us > 0 && elapsed_us < WS_MIN_BROADCAST_INTERVAL_US) {
        if (s_ws_debounce_timer) {
            int64_t delay_us = WS_MIN_BROADCAST_INTERVAL_US - elapsed_us;
            if (delay_us < 1000) {
                delay_us = 1000;
            }
            (void)esp_timer_stop(s_ws_debounce_timer);
            (void)esp_timer_start_once(s_ws_debounce_timer, (uint64_t)delay_us);
        }
        return;
    }

    size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)s_ws_json_buf;
    ws_pkt.len = json_len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            esp_err_t ret = httpd_ws_send_frame_async(s_server, ws_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "WS send failed (%s), removing client %d", esp_err_to_name(ret), ws_fds[i]);
                ws_fds[i] = -1;
            }
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }

    if (json_len < sizeof(s_last_ws_json)) {
        memcpy(s_last_ws_json, s_ws_json_buf, json_len);
        s_last_ws_json[json_len] = '\0';
        s_last_ws_json_len = json_len;
    } else {
        s_last_ws_json_len = 0;
    }
    s_last_ws_send_us = now_us;

    size_t heap_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGD(TAG, "WS broadcast heap: before=%u after=%u delta=%d",
             (unsigned)heap_before, (unsigned)heap_after, (int)(heap_after - heap_before));
}

int ws_manager_get_client_count(void)
{
    int count = 0;
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            count++;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return count;
}
