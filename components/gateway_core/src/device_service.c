#include "device_service_internal.h"

#include <stdlib.h>
#include <string.h>

#include "device_service_rules.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "DEV_SERVICE";
static const char *s_default_device_name_prefix = "Пристрій";

esp_err_t device_service_create(device_service_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    device_service_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    *out_handle = handle;
    return ESP_OK;
}

void device_service_destroy(device_service_handle_t handle)
{
    if (!handle) {
        return;
    }

    device_service_events_unregister_announce_handler(handle);
    if (handle->devices_mutex) {
        vSemaphoreDelete(handle->devices_mutex);
        handle->devices_mutex = NULL;
    }
    handle->device_count = 0;
    memset(handle->devices, 0, sizeof(handle->devices));
    free(handle);
}

esp_err_t device_service_init(device_service_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->devices_mutex) {
        handle->devices_mutex = xSemaphoreCreateMutex();
        if (!handle->devices_mutex) {
            ESP_LOGE(TAG, "Failed to create devices mutex");
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreTake(handle->devices_mutex, portMAX_DELAY);
        (void)device_service_storage_load_locked(handle);
        xSemaphoreGive(handle->devices_mutex);
    }

    return device_service_events_register_announce_handler(handle);
}

void device_service_add_with_ieee(device_service_handle_t handle, uint16_t addr, gateway_ieee_addr_t ieee)
{
    if (!handle) {
        return;
    }

    if (handle->devices_mutex) {
        xSemaphoreTake(handle->devices_mutex, portMAX_DELAY);
    }

    device_service_rules_result_t upsert_result = device_service_rules_upsert(
        handle->devices, &handle->device_count, MAX_DEVICES, addr, ieee, s_default_device_name_prefix);
    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_UPDATED) {
        ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
        if (handle->devices_mutex) {
            xSemaphoreGive(handle->devices_mutex);
        }
        return;
    }

    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_ADDED) {
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, handle->device_count);
        (void)device_service_storage_save_locked(handle);
    } else if (upsert_result == DEVICE_SERVICE_RULES_RESULT_LIMIT_REACHED) {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
    } else if (upsert_result == DEVICE_SERVICE_RULES_RESULT_INVALID_ARG) {
        ESP_LOGW(TAG, "Invalid input for add/update operation");
    }

    if (handle->devices_mutex) {
        xSemaphoreGive(handle->devices_mutex);
    }
    device_service_events_post_list_changed();
}

void device_service_update_name(device_service_handle_t handle, uint16_t addr, const char *new_name)
{
    if (!handle || !new_name) {
        return;
    }
    bool renamed = false;

    if (handle->devices_mutex) {
        xSemaphoreTake(handle->devices_mutex, portMAX_DELAY);
    }

    renamed = device_service_rules_rename(handle->devices, handle->device_count, addr, new_name);
    if (renamed) {
        int idx = device_service_rules_find_index_by_short_addr(handle->devices, handle->device_count, addr);
        if (idx >= 0) {
            ESP_LOGI(TAG, "Device 0x%04x renamed to '%s'", addr, handle->devices[idx].name);
        }
        (void)device_service_storage_save_locked(handle);
    }

    if (handle->devices_mutex) {
        xSemaphoreGive(handle->devices_mutex);
    }
    device_service_events_post_list_changed();
}

void device_service_delete(device_service_handle_t handle, uint16_t addr)
{
    if (!handle) {
        return;
    }

    bool deleted = false;
    uint16_t deleted_short_addr = 0;
    gateway_ieee_addr_t deleted_ieee = {0};

    if (handle->devices_mutex) {
        xSemaphoreTake(handle->devices_mutex, portMAX_DELAY);
    }

    zb_device_t deleted_device = {0};
    deleted = device_service_rules_delete_by_short_addr(handle->devices, &handle->device_count, addr, &deleted_device);
    if (deleted) {
        deleted_short_addr = deleted_device.short_addr;
        memcpy(deleted_ieee, deleted_device.ieee_addr, sizeof(deleted_ieee));
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, handle->device_count);
        (void)device_service_storage_save_locked(handle);
    }

    if (handle->devices_mutex) {
        xSemaphoreGive(handle->devices_mutex);
    }
    if (deleted) {
        device_service_events_post_delete_request(deleted_short_addr, deleted_ieee);
    }
    device_service_events_post_list_changed();
}

int device_service_get_snapshot(device_service_handle_t handle, zb_device_t *out, size_t max_items)
{
    if (!handle || !out || max_items == 0) {
        return 0;
    }

    if (handle->devices_mutex) {
        xSemaphoreTake(handle->devices_mutex, portMAX_DELAY);
    }

    int count = handle->device_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, handle->devices, sizeof(zb_device_t) * (size_t)count);
    }

    if (handle->devices_mutex) {
        xSemaphoreGive(handle->devices_mutex);
    }
    return count;
}
