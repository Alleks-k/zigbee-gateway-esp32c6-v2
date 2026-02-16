#include "device_service_internal.h"

#include <string.h>

#include "esp_log.h"
#include "gateway_events.h"

static const char *TAG = "DEV_SERVICE_EVT";

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
