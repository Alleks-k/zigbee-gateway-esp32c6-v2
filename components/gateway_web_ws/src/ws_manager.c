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

#include <stdlib.h>
#include <string.h>

static const char *TAG = "WS_MANAGER";

static ws_manager_handle_t s_provider_ws_manager = NULL;

void ws_broadcast_status_with_handle(ws_manager_handle_t handle);

#if CONFIG_GATEWAY_SELF_TEST_APP
static void ws_manager_reset_transport_to_defaults(ws_manager_handle_t handle)
{
    ws_manager_reset_transport_ops_for_test_with_handle(handle);
}
#endif

static bool ws_manager_metrics_provider_adapter(api_ws_runtime_metrics_t *out_metrics)
{
    return ws_manager_metrics_provider(s_provider_ws_manager, out_metrics);
}

static uint32_t ws_manager_client_count_provider_adapter(void)
{
    return ws_manager_client_count_provider(s_provider_ws_manager);
}

static void ws_debounce_timer_cb(void *arg)
{
    ws_manager_handle_t handle = (ws_manager_handle_t)arg;
    ws_broadcast_status_with_handle(handle);
}

static void ws_periodic_timer_cb(void *arg)
{
    ws_manager_handle_t handle = (ws_manager_handle_t)arg;
    ws_broadcast_status_with_handle(handle);
}

static void device_list_changed_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ws_manager_handle_t handle = (ws_manager_handle_t)arg;
    (void)event_data;
    if (event_base == GATEWAY_EVENT && event_id == GATEWAY_EVENT_DEVICE_LIST_CHANGED) {
        ws_broadcast_status_with_handle(handle);
    }
}

static void lqi_state_changed_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ws_manager_handle_t handle = (ws_manager_handle_t)arg;
    (void)event_data;
    if (event_base == GATEWAY_EVENT && event_id == GATEWAY_EVENT_LQI_STATE_CHANGED) {
        ws_broadcast_status_with_handle(handle);
    }
}

esp_err_t ws_manager_create(ws_manager_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    ws_manager_ctx_t *handle = (ws_manager_ctx_t *)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        handle->ws_fds[i] = -1;
    }
#if CONFIG_GATEWAY_SELF_TEST_APP
    ws_manager_reset_transport_to_defaults(handle);
#endif
    *out_handle = handle;
    return ESP_OK;
}

void ws_manager_destroy(ws_manager_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (handle->list_changed_handler) {
        (void)esp_event_handler_instance_unregister(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, handle->list_changed_handler);
        handle->list_changed_handler = NULL;
    }
    if (handle->lqi_changed_handler) {
        (void)esp_event_handler_instance_unregister(
            GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, handle->lqi_changed_handler);
        handle->lqi_changed_handler = NULL;
    }

    if (handle->ws_debounce_timer) {
        (void)esp_timer_stop(handle->ws_debounce_timer);
        (void)esp_timer_delete(handle->ws_debounce_timer);
        handle->ws_debounce_timer = NULL;
    }
    if (handle->ws_periodic_timer) {
        (void)esp_timer_stop(handle->ws_periodic_timer);
        (void)esp_timer_delete(handle->ws_periodic_timer);
        handle->ws_periodic_timer = NULL;
    }

    if (handle->ws_broadcast_mutex) {
        vSemaphoreDelete(handle->ws_broadcast_mutex);
        handle->ws_broadcast_mutex = NULL;
    }
    if (handle->ws_mutex) {
        vSemaphoreDelete(handle->ws_mutex);
        handle->ws_mutex = NULL;
    }

    if (s_provider_ws_manager == handle) {
        s_provider_ws_manager = NULL;
    }
    free(handle);
}

void ws_manager_init_with_handle(ws_manager_handle_t handle, httpd_handle_t server)
{
    if (!handle) {
        return;
    }

    handle->server = server;
    s_provider_ws_manager = handle;
    api_usecases_set_ws_client_count_provider(ws_manager_client_count_provider_adapter);
    api_usecases_set_ws_metrics_provider(ws_manager_metrics_provider_adapter);

    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        handle->ws_fds[i] = -1;
    }

    if (!handle->ws_mutex) {
        handle->ws_mutex = xSemaphoreCreateMutex();
        if (!handle->ws_mutex) {
            ESP_LOGE(TAG, "Failed to create WS mutex");
        }
    }
    if (!handle->ws_broadcast_mutex) {
        handle->ws_broadcast_mutex = xSemaphoreCreateMutex();
        if (!handle->ws_broadcast_mutex) {
            ESP_LOGE(TAG, "Failed to create WS broadcast mutex");
        }
    }

    if (handle->list_changed_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, device_list_changed_handler, handle, &handle->list_changed_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_LIST_CHANGED handler: %s", esp_err_to_name(ret));
        }
    }
    if (handle->lqi_changed_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, lqi_state_changed_handler, handle, &handle->lqi_changed_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register LQI_STATE_CHANGED handler: %s", esp_err_to_name(ret));
        }
    }

    if (handle->ws_debounce_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = ws_debounce_timer_cb,
            .arg = handle,
            .name = "ws_debounce"
        };
        esp_err_t ret = esp_timer_create(&timer_args, &handle->ws_debounce_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create WS debounce timer: %s", esp_err_to_name(ret));
        }
    }

    if (handle->ws_periodic_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = ws_periodic_timer_cb,
            .arg = handle,
            .name = "ws_periodic"
        };
        esp_err_t ret = esp_timer_create(&timer_args, &handle->ws_periodic_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create WS periodic timer: %s", esp_err_to_name(ret));
        }
    }

    handle->last_ws_devices_json_len = 0;
    handle->last_ws_devices_send_us = 0;
    handle->last_ws_health_json_len = 0;
    handle->last_ws_health_send_us = 0;
    handle->last_ws_lqi_json_len = 0;
    handle->last_ws_lqi_send_us = 0;
    handle->ws_seq = 0;
    memset(&handle->ws_metrics, 0, sizeof(handle->ws_metrics));
}

void ws_httpd_close_fn_with_handle(ws_manager_handle_t handle, httpd_handle_t hd, int sockfd)
{
    if (!handle) {
        return;
    }
    (void)hd;
    ws_manager_remove_fd_internal(handle, sockfd);
    ws_manager_transport_close_socket(handle, sockfd);
}

esp_err_t ws_handler_with_handle(ws_manager_handle_t handle, httpd_req_t *req)
{
    if (!handle || !req) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WS connection");
        int fd = ws_manager_transport_req_to_sockfd(handle, req);
        bool added = ws_manager_add_fd(handle, fd);
        if (!added) {
            ESP_LOGW(TAG, "WS client rejected: max clients reached (%d)", MAX_WS_CLIENTS);
            gateway_error_ring_add("ws", (int32_t)ESP_ERR_NO_MEM, "client rejected: max clients");
            (void)ws_manager_transport_resp_set_status(handle, req, "503 Service Unavailable");
            (void)ws_manager_transport_resp_send(handle, req, "WS clients limit reached", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }

        ws_manager_note_connection(handle);
        if (handle->ws_periodic_timer) {
            (void)esp_timer_stop(handle->ws_periodic_timer);
            (void)esp_timer_start_periodic(handle->ws_periodic_timer, 1000 * 1000);
        }
        ws_broadcast_status_with_handle(handle);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = ws_manager_transport_recv_frame(handle, req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        gateway_error_ring_add("ws", (int32_t)ret, "recv_frame failed");
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = ws_manager_transport_req_to_sockfd(handle, req);
        ws_manager_remove_fd_internal(handle, fd);
    }
    return ESP_OK;
}
