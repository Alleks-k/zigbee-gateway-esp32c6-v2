#include "device_service.h"

#include <stdio.h>
#include <string.h>

#include "device_repository.h"
#include "device_service_rules.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "gateway_events.h"

static const char *TAG = "DEV_SERVICE";

static SemaphoreHandle_t s_devices_mutex = NULL;
static zb_device_t s_devices[MAX_DEVICES];
static int s_device_count = 0;
static esp_event_handler_instance_t s_dev_announce_handler = NULL;
static const char *s_default_device_name_prefix = "Пристрій";

static void save_devices_locked(void)
{
    esp_err_t err = device_repository_save(s_devices, MAX_DEVICES, s_device_count);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device list successfully saved");
    } else {
        ESP_LOGW(TAG, "Failed to save devices: %s", esp_err_to_name(err));
    }
}

static void load_devices_locked(void)
{
    bool loaded = false;
    int count = 0;
    esp_err_t err = device_repository_load(s_devices, MAX_DEVICES, &count, &loaded);
    if (err == ESP_OK && loaded) {
        s_device_count = count;
        ESP_LOGI(TAG, "Loaded %d devices", s_device_count);
    } else if (err == ESP_OK) {
        s_device_count = 0;
        ESP_LOGW(TAG, "No device data found (first boot?)");
    } else {
        s_device_count = 0;
        ESP_LOGW(TAG, "Failed to load device data: %s", esp_err_to_name(err));
    }
}

static void post_device_list_changed_event(void)
{
    esp_err_t ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, NULL, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post DEVICE_LIST_CHANGED event: %s", esp_err_to_name(ret));
    }
}

static void post_device_delete_request_event(uint16_t short_addr, gateway_ieee_addr_t ieee_addr)
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

static void device_announce_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base != GATEWAY_EVENT || event_id != GATEWAY_EVENT_DEVICE_ANNOUNCE || !event_data) {
        return;
    }

    gateway_device_announce_event_t *evt = (gateway_device_announce_event_t *)event_data;
    device_service_add_with_ieee(evt->short_addr, evt->ieee_addr);
}

esp_err_t device_service_init(void)
{
    if (!s_devices_mutex) {
        s_devices_mutex = xSemaphoreCreateMutex();
        if (!s_devices_mutex) {
            ESP_LOGE(TAG, "Failed to create devices mutex");
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
        load_devices_locked();
        xSemaphoreGive(s_devices_mutex);
    }

    if (!s_dev_announce_handler) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, device_announce_event_handler, NULL, &s_dev_announce_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_ANNOUNCE handler: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    return ESP_OK;
}

void device_service_add_with_ieee(uint16_t addr, gateway_ieee_addr_t ieee)
{
    device_service_rules_result_t upsert_result = DEVICE_SERVICE_RULES_RESULT_INVALID_ARG;

    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    upsert_result = device_service_rules_upsert(
        s_devices, &s_device_count, MAX_DEVICES, addr, ieee, s_default_device_name_prefix);
    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_UPDATED) {
        ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
        if (s_devices_mutex) {
            xSemaphoreGive(s_devices_mutex);
        }
        return;
    }

    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_ADDED) {
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, s_device_count);
        save_devices_locked();
    } else if (upsert_result == DEVICE_SERVICE_RULES_RESULT_LIMIT_REACHED) {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
    } else if (upsert_result == DEVICE_SERVICE_RULES_RESULT_INVALID_ARG) {
        ESP_LOGW(TAG, "Invalid input for add/update operation");
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    post_device_list_changed_event();
}

void device_service_update_name(uint16_t addr, const char *new_name)
{
    if (!new_name) {
        return;
    }
    bool renamed = false;

    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    renamed = device_service_rules_rename(s_devices, s_device_count, addr, new_name);
    if (renamed) {
        int idx = device_service_rules_find_index_by_short_addr(s_devices, s_device_count, addr);
        if (idx >= 0) {
            ESP_LOGI(TAG, "Device 0x%04x renamed to '%s'", addr, s_devices[idx].name);
        }
        save_devices_locked();
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    post_device_list_changed_event();
}

void device_service_delete(uint16_t addr)
{
    bool deleted = false;
    gateway_device_delete_request_event_t req_evt = {0};

    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    zb_device_t deleted_device = {0};
    deleted = device_service_rules_delete_by_short_addr(s_devices, &s_device_count, addr, &deleted_device);
    if (deleted) {
        req_evt.short_addr = deleted_device.short_addr;
        memcpy(req_evt.ieee_addr, deleted_device.ieee_addr, sizeof(req_evt.ieee_addr));
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, s_device_count);
        save_devices_locked();
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    if (deleted) {
        post_device_delete_request_event(req_evt.short_addr, req_evt.ieee_addr);
    }
    post_device_list_changed_event();
}

int device_service_get_snapshot(zb_device_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }

    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    int count = s_device_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, s_devices, sizeof(zb_device_t) * (size_t)count);
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    return count;
}

void device_service_lock(void)
{
    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }
}

void device_service_unlock(void)
{
    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
}
