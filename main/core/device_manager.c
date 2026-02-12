#include "device_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "gateway_events.h"

static const char *TAG = "DEV_MANAGER";
static SemaphoreHandle_t devices_mutex = NULL;
static zb_device_t devices[MAX_DEVICES];
static int device_count = 0;
static esp_event_handler_instance_t s_dev_announce_handler = NULL;

static void save_devices_to_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_i32(my_handle, "dev_count", device_count);
        nvs_set_blob(my_handle, "dev_list", devices, sizeof(zb_device_t) * MAX_DEVICES);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device list successfully saved to NVS");
        }
    }
}

static void load_devices_from_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(my_handle, "dev_count", &count);
        device_count = (int)count;
        
        size_t size = sizeof(zb_device_t) * MAX_DEVICES;
        nvs_get_blob(my_handle, "dev_list", devices, &size);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Loaded %d devices from NVS", device_count);
    } else {
        ESP_LOGW(TAG, "No device data found in NVS (First boot?)");
        device_count = 0;
    }
}

static void post_device_list_changed_event(void)
{
    esp_err_t ret = esp_event_post(ZGW_EVENT, ZGW_EVENT_DEVICE_LIST_CHANGED, NULL, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post DEVICE_LIST_CHANGED event: %s", esp_err_to_name(ret));
    }
}

static void post_device_delete_request_event(uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr)
{
    zgw_device_delete_request_event_t evt = {
        .short_addr = short_addr,
    };
    memcpy(evt.ieee_addr, ieee_addr, sizeof(evt.ieee_addr));
    esp_err_t ret = esp_event_post(ZGW_EVENT, ZGW_EVENT_DEVICE_DELETE_REQUEST, &evt, sizeof(evt), 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post DEVICE_DELETE_REQUEST event: %s", esp_err_to_name(ret));
    }
}

static void device_announce_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base != ZGW_EVENT || event_id != ZGW_EVENT_DEVICE_ANNOUNCE || event_data == NULL) {
        return;
    }

    zgw_device_announce_event_t *evt = (zgw_device_announce_event_t *)event_data;
    add_device_with_ieee(evt->short_addr, evt->ieee_addr);
}

void update_device_name(uint16_t addr, const char *new_name) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            strncpy(devices[i].name, new_name, sizeof(devices[i].name) - 1);
            devices[i].name[sizeof(devices[i].name) - 1] = '\0'; // Гарантуємо null-термінатор
            ESP_LOGI(TAG, "Device 0x%04x renamed to '%s'", addr, devices[i].name);
            save_devices_to_nvs();
            break;
        }
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    post_device_list_changed_event();
}

void device_manager_init(void) {
    if (devices_mutex == NULL) {
        devices_mutex = xSemaphoreCreateMutex();
        load_devices_from_nvs();
    }
    if (s_dev_announce_handler == NULL) {
        esp_err_t ret = esp_event_handler_instance_register(
            ZGW_EVENT, ZGW_EVENT_DEVICE_ANNOUNCE, device_announce_event_handler, NULL, &s_dev_announce_handler);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register DEVICE_ANNOUNCE handler: %s", esp_err_to_name(ret));
        }
    }
}

void add_device_with_ieee(uint16_t addr, esp_zb_ieee_addr_t ieee) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
            memcpy(devices[i].ieee_addr, ieee, sizeof(esp_zb_ieee_addr_t));
            if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
            return;
        }
    }

    if (device_count < MAX_DEVICES) {
        devices[device_count].short_addr = addr;
        memcpy(devices[device_count].ieee_addr, ieee, sizeof(esp_zb_ieee_addr_t));
        snprintf(devices[device_count].name, sizeof(devices[device_count].name), "Пристрій 0x%04x", addr);
        device_count++;
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, device_count);
        save_devices_to_nvs();
    } else {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    post_device_list_changed_event();
}

void delete_device(uint16_t addr) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    int found_idx = -1;
    zgw_device_delete_request_event_t req_evt = {0};
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
        save_devices_to_nvs();
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
