#include "device_service_internal.h"

#include <string.h>

#include "esp_log.h"
#include "gateway_events.h"

static const char *TAG = "DEV_SERVICE_EVT";

static void device_announce_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    device_service_handle_t handle = (device_service_handle_t)arg;
    if (!handle) {
        return;
    }
    if (event_base != GATEWAY_EVENT || event_id != GATEWAY_EVENT_DEVICE_ANNOUNCE || !event_data) {
        return;
    }

    gateway_device_announce_event_t *evt = (gateway_device_announce_event_t *)event_data;
    device_service_add_with_ieee(handle, evt->short_addr, evt->ieee_addr);
}

esp_err_t device_service_events_register_announce_handler(device_service_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((esp_event_handler_instance_t)handle->dev_announce_handler) {
        return ESP_OK;
    }

    esp_event_handler_instance_t handler = NULL;
    esp_err_t ret = esp_event_handler_instance_register(
        GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, device_announce_event_handler, handle, &handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register DEVICE_ANNOUNCE handler: %s", esp_err_to_name(ret));
    } else {
        handle->dev_announce_handler = (void *)handler;
    }
    return ret;
}

void device_service_events_unregister_announce_handler(device_service_handle_t handle)
{
    if (!handle || !handle->dev_announce_handler) {
        return;
    }

    (void)esp_event_handler_instance_unregister(
        GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, (esp_event_handler_instance_t)handle->dev_announce_handler);
    handle->dev_announce_handler = NULL;
}

void device_service_events_post_list_changed(void)
{
    esp_err_t ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, NULL, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post DEVICE_LIST_CHANGED event: %s", esp_err_to_name(ret));
    }
}

void device_service_events_post_delete_request(uint16_t short_addr, gateway_ieee_addr_t ieee_addr)
{
    gateway_device_delete_request_event_t evt = {
        .short_addr = short_addr,
    };
    memcpy(evt.ieee_addr, ieee_addr, sizeof(evt.ieee_addr));
    esp_err_t ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_DELETE_REQUEST, &evt, sizeof(evt), 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post DEVICE_DELETE_REQUEST event: %s", esp_err_to_name(ret));
    }
}
