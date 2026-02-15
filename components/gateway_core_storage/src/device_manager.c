#include "device_manager.h"
#include "esp_log.h"
#include "settings_manager.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "gateway_events.h"
#include "gateway_state.h"
#include "cJSON.h"

static const char *TAG = "DEV_MANAGER";
static SemaphoreHandle_t devices_mutex = NULL;
static zb_device_t devices[MAX_DEVICES];
static int device_count = 0;
static esp_event_handler_instance_t s_dev_announce_handler = NULL;

/* Caller must hold devices_mutex. */
static void sync_gateway_state_devices_locked(void)
{
    esp_err_t err = gateway_state_set_devices(devices, device_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to sync device snapshot to gateway_state: %s", esp_err_to_name(err));
    }
}

/* Caller must hold devices_mutex. */
static void save_devices_to_nvs_locked(void) {
    esp_err_t err = settings_manager_save_devices(devices, MAX_DEVICES, device_count);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device list successfully saved to NVS");
    } else {
        ESP_LOGW(TAG, "Failed to save devices to NVS: %s", esp_err_to_name(err));
    }
}

/* Caller must hold devices_mutex. */
static void load_devices_from_nvs_locked(void) {
    bool loaded = false;
    int count = 0;
    esp_err_t err = settings_manager_load_devices(devices, MAX_DEVICES, &count, &loaded);
    if (err == ESP_OK && loaded) {
        device_count = count;
        ESP_LOGI(TAG, "Loaded %d devices from NVS", device_count);
    } else if (err == ESP_OK) {
        device_count = 0;
        ESP_LOGW(TAG, "No device data found in NVS (First boot?)");
    } else {
        device_count = 0;
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
    if (event_base != GATEWAY_EVENT || event_id != GATEWAY_EVENT_DEVICE_ANNOUNCE || event_data == NULL) {
        return;
    }

    gateway_device_announce_event_t *evt = (gateway_device_announce_event_t *)event_data;
    add_device_with_ieee(evt->short_addr, evt->ieee_addr);
}

void update_device_name(uint16_t addr, const char *new_name) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            strncpy(devices[i].name, new_name, sizeof(devices[i].name) - 1);
            devices[i].name[sizeof(devices[i].name) - 1] = '\0'; // Гарантуємо null-термінатор
            ESP_LOGI(TAG, "Device 0x%04x renamed to '%s'", addr, devices[i].name);
            sync_gateway_state_devices_locked();
            save_devices_to_nvs_locked();
            break;
        }
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    post_device_list_changed_event();
}

esp_err_t device_manager_init(void) {
    if (devices_mutex == NULL) {
        devices_mutex = xSemaphoreCreateMutex();
        if (devices_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create devices mutex");
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreTake(devices_mutex, portMAX_DELAY);
        load_devices_from_nvs_locked();
        sync_gateway_state_devices_locked();
        xSemaphoreGive(devices_mutex);
    }
    if (s_dev_announce_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, device_announce_event_handler, NULL, &s_dev_announce_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_ANNOUNCE handler: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

void add_device_with_ieee(uint16_t addr, gateway_ieee_addr_t ieee) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
            memcpy(devices[i].ieee_addr, ieee, sizeof(gateway_ieee_addr_t));
            sync_gateway_state_devices_locked();
            if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
            return;
        }
    }

    if (device_count < MAX_DEVICES) {
        devices[device_count].short_addr = addr;
        memcpy(devices[device_count].ieee_addr, ieee, sizeof(gateway_ieee_addr_t));
        snprintf(devices[device_count].name, sizeof(devices[device_count].name), "Пристрій 0x%04x", addr);
        device_count++;
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, device_count);
        sync_gateway_state_devices_locked();
        save_devices_to_nvs_locked();
    } else {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    post_device_list_changed_event();
}

void delete_device(uint16_t addr) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    int found_idx = -1;
    gateway_device_delete_request_event_t req_evt = {0};
    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            found_idx = i;
            req_evt.short_addr = devices[i].short_addr;
            memcpy(req_evt.ieee_addr, devices[i].ieee_addr, sizeof(req_evt.ieee_addr));
            break;
        }
    }

    if (found_idx != -1) {
        for (int i = found_idx; i < device_count - 1; i++) {
            devices[i] = devices[i + 1];
        }
        device_count--;
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, device_count);
        sync_gateway_state_devices_locked();
        save_devices_to_nvs_locked();
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    if (found_idx != -1) {
        post_device_delete_request_event(req_evt.short_addr, req_evt.ieee_addr);
    }
    post_device_list_changed_event();
}

cJSON* device_manager_get_json_list(void) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);
    cJSON *dev_list = cJSON_CreateArray();
    for (int i = 0; i < device_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", devices[i].name);
        cJSON_AddNumberToObject(item, "short_addr", devices[i].short_addr);
        cJSON_AddItemToArray(dev_list, item);
    }
    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    return dev_list;
}

int device_manager_get_snapshot(zb_device_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }

    if (devices_mutex != NULL) {
        xSemaphoreTake(devices_mutex, portMAX_DELAY);
    }
    int count = device_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, devices, sizeof(zb_device_t) * (size_t)count);
    }
    if (devices_mutex != NULL) {
        xSemaphoreGive(devices_mutex);
    }
    return count;
}
