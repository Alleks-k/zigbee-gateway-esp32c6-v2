#include "device_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "web_server.h"         // Для ws_broadcast_status
#include "esp_zigbee_gateway.h" // Для send_leave_command

static const char *TAG = "DEV_MANAGER";
static SemaphoreHandle_t devices_mutex = NULL;
static zb_device_t devices[MAX_DEVICES];
static int device_count = 0;

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
    ws_broadcast_status();
}

void device_manager_init(void) {
    if (devices_mutex == NULL) {
        devices_mutex = xSemaphoreCreateMutex();
        load_devices_from_nvs();
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
    ws_broadcast_status();
}

void delete_device(uint16_t addr) {
    if (devices_mutex != NULL) xSemaphoreTake(devices_mutex, portMAX_DELAY);

    int found_idx = -1;
    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            found_idx = i;
            break;
        }
    }

    if (found_idx != -1) {
        esp_zb_bdb_open_network(0); // Закриваємо мережу
        send_leave_command(devices[found_idx].short_addr, devices[found_idx].ieee_addr);

        for (int i = found_idx; i < device_count - 1; i++) {
            devices[i] = devices[i + 1];
        }
        device_count--;
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, device_count);
        save_devices_to_nvs();
    }

    if (devices_mutex != NULL) xSemaphoreGive(devices_mutex);
    ws_broadcast_status();
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
