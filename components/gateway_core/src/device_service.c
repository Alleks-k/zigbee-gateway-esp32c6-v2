#include "device_service.h"

#include <stdio.h>
#include <string.h>

#include "device_repository.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "gateway_events.h"
#include "gateway_state.h"

static const char *TAG = "DEV_SERVICE";

static SemaphoreHandle_t s_devices_mutex = NULL;
static zb_device_t s_devices[MAX_DEVICES];
static int s_device_count = 0;
static esp_event_handler_instance_t s_dev_announce_handler = NULL;

static void sync_gateway_state_devices_locked(void)
{
    esp_err_t err = gateway_state_set_devices(s_devices, s_device_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to sync device snapshot to gateway_state: %s", esp_err_to_name(err));
    }
}

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
        sync_gateway_state_devices_locked();
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
    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].short_addr == addr) {
            ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
            memcpy(s_devices[i].ieee_addr, ieee, sizeof(gateway_ieee_addr_t));
            sync_gateway_state_devices_locked();
            if (s_devices_mutex) {
                xSemaphoreGive(s_devices_mutex);
            }
            return;
        }
    }

    if (s_device_count < MAX_DEVICES) {
        s_devices[s_device_count].short_addr = addr;
        memcpy(s_devices[s_device_count].ieee_addr, ieee, sizeof(gateway_ieee_addr_t));
        snprintf(s_devices[s_device_count].name, sizeof(s_devices[s_device_count].name), "Пристрій 0x%04x", addr);
        s_device_count++;
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, s_device_count);
        sync_gateway_state_devices_locked();
        save_devices_locked();
    } else {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
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

    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].short_addr == addr) {
            strncpy(s_devices[i].name, new_name, sizeof(s_devices[i].name) - 1);
            s_devices[i].name[sizeof(s_devices[i].name) - 1] = '\0';
            ESP_LOGI(TAG, "Device 0x%04x renamed to '%s'", addr, s_devices[i].name);
            sync_gateway_state_devices_locked();
            save_devices_locked();
            break;
        }
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    post_device_list_changed_event();
}

void device_service_delete(uint16_t addr)
{
    if (s_devices_mutex) {
        xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    }

    int found_idx = -1;
    gateway_device_delete_request_event_t req_evt = {0};
    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].short_addr == addr) {
            found_idx = i;
            req_evt.short_addr = s_devices[i].short_addr;
            memcpy(req_evt.ieee_addr, s_devices[i].ieee_addr, sizeof(req_evt.ieee_addr));
            break;
        }
    }

    if (found_idx != -1) {
        for (int i = found_idx; i < s_device_count - 1; i++) {
            s_devices[i] = s_devices[i + 1];
        }
        s_device_count--;
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, s_device_count);
        sync_gateway_state_devices_locked();
        save_devices_locked();
    }

    if (s_devices_mutex) {
        xSemaphoreGive(s_devices_mutex);
    }
    if (found_idx != -1) {
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

esp_err_t device_manager_init(void)
{
    return device_service_init();
}

void add_device_with_ieee(uint16_t addr, gateway_ieee_addr_t ieee)
{
    device_service_add_with_ieee(addr, ieee);
}

void update_device_name(uint16_t addr, const char *new_name)
{
    device_service_update_name(addr, new_name);
}

void delete_device(uint16_t addr)
{
    device_service_delete(addr);
}

int device_manager_get_snapshot(zb_device_t *out, size_t max_items)
{
    return device_service_get_snapshot(out, max_items);
}

void device_manager_lock(void)
{
    device_service_lock();
}

void device_manager_unlock(void)
{
    device_service_unlock();
}
