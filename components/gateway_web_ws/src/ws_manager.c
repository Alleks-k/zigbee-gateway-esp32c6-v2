#include "ws_manager.h"

#include "api_usecases.h"
#include "error_ring.h"
#include "gateway_events.h"
#include "ws_manager_internal.h"
#include "ws_manager_state.h"
#include "ws_manager_transport.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "WS_MANAGER";

int ws_fds[MAX_WS_CLIENTS];
httpd_handle_t s_server = NULL;
SemaphoreHandle_t s_ws_mutex = NULL;
SemaphoreHandle_t s_ws_broadcast_mutex = NULL;
esp_event_handler_instance_t s_list_changed_handler = NULL;
esp_event_handler_instance_t s_lqi_changed_handler = NULL;
esp_timer_handle_t s_ws_debounce_timer = NULL;
esp_timer_handle_t s_ws_periodic_timer = NULL;
char s_ws_devices_json_buf[WS_JSON_BUF_SIZE];
char s_last_ws_devices_json[WS_JSON_BUF_SIZE];
size_t s_last_ws_devices_json_len = 0;
int64_t s_last_ws_devices_send_us = 0;
char s_ws_health_json_buf[WS_JSON_BUF_SIZE];
char s_last_ws_health_json[WS_JSON_BUF_SIZE];
size_t s_last_ws_health_json_len = 0;
int64_t s_last_ws_health_send_us = 0;
char s_ws_lqi_json_buf[WS_JSON_BUF_SIZE];
char s_last_ws_lqi_json[WS_JSON_BUF_SIZE];
size_t s_last_ws_lqi_json_len = 0;
int64_t s_last_ws_lqi_send_us = 0;
char s_ws_frame_buf[WS_FRAME_BUF_SIZE];
uint32_t s_ws_seq = 0;
api_ws_runtime_metrics_t s_ws_metrics = {0};

static void ws_debounce_timer_cb(void *arg)
{
    (void)arg;
    ws_broadcast_status();
}

static void ws_periodic_timer_cb(void *arg)
{
    (void)arg;
    ws_broadcast_status();
}

static void device_list_changed_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == GATEWAY_EVENT && event_id == GATEWAY_EVENT_DEVICE_LIST_CHANGED) {
        ws_broadcast_status();
    }
}

static void lqi_state_changed_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == GATEWAY_EVENT && event_id == GATEWAY_EVENT_LQI_STATE_CHANGED) {
        ws_broadcast_status();
    }
}

void ws_manager_init(httpd_handle_t server)
{
    s_server = server;
    api_usecases_set_ws_client_count_provider(ws_manager_client_count_provider);
    api_usecases_set_ws_metrics_provider(ws_manager_metrics_provider);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        ws_fds[i] = -1;
    }

    if (!s_ws_mutex) {
        s_ws_mutex = xSemaphoreCreateMutex();
        if (!s_ws_mutex) {
            ESP_LOGE(TAG, "Failed to create WS mutex");
        }
    }
    if (!s_ws_broadcast_mutex) {
        s_ws_broadcast_mutex = xSemaphoreCreateMutex();
        if (!s_ws_broadcast_mutex) {
            ESP_LOGE(TAG, "Failed to create WS broadcast mutex");
        }
    }

    if (s_list_changed_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, device_list_changed_handler, NULL, &s_list_changed_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_LIST_CHANGED handler: %s", esp_err_to_name(ret));
        }
    }
    if (s_lqi_changed_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, lqi_state_changed_handler, NULL, &s_lqi_changed_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register LQI_STATE_CHANGED handler: %s", esp_err_to_name(ret));
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

    if (s_ws_periodic_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = ws_periodic_timer_cb,
            .arg = NULL,
            .name = "ws_periodic"
        };
        esp_err_t ret = esp_timer_create(&timer_args, &s_ws_periodic_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create WS periodic timer: %s", esp_err_to_name(ret));
        }
    }

    s_last_ws_devices_json_len = 0;
    s_last_ws_devices_send_us = 0;
    s_last_ws_health_json_len = 0;
    s_last_ws_health_send_us = 0;
    s_last_ws_lqi_json_len = 0;
    s_last_ws_lqi_send_us = 0;
    s_ws_seq = 0;
    memset(&s_ws_metrics, 0, sizeof(s_ws_metrics));
}

void ws_httpd_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    ws_manager_remove_fd_internal(sockfd);
    ws_manager_transport_close_socket(sockfd);
}

esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WS connection");
        int fd = ws_manager_transport_req_to_sockfd(req);
        bool added = ws_manager_add_fd(fd);
        if (!added) {
            ESP_LOGW(TAG, "WS client rejected: max clients reached (%d)", MAX_WS_CLIENTS);
            gateway_error_ring_add("ws", (int32_t)ESP_ERR_NO_MEM, "client rejected: max clients");
            (void)ws_manager_transport_resp_set_status(req, "503 Service Unavailable");
            (void)ws_manager_transport_resp_send(req, "WS clients limit reached", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }

        ws_manager_note_connection();
        if (s_ws_periodic_timer) {
            (void)esp_timer_stop(s_ws_periodic_timer);
            (void)esp_timer_start_periodic(s_ws_periodic_timer, 1000 * 1000);
        }
        ws_broadcast_status();
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = ws_manager_transport_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        gateway_error_ring_add("ws", (int32_t)ret, "recv_frame failed");
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = ws_manager_transport_req_to_sockfd(req);
        ws_manager_remove_fd_internal(fd);
    }
    return ESP_OK;
}
