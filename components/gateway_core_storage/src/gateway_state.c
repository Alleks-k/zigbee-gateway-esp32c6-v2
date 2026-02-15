#include "state_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static SemaphoreHandle_t s_state_mutex = NULL;
static gateway_network_state_t s_network_state = {0};
static gateway_wifi_state_t s_wifi_state = {0};
static zb_device_t s_devices[MAX_DEVICES];
static gateway_device_lqi_state_t s_device_lqi[MAX_DEVICES];
static int s_device_count = 0;
static int s_device_lqi_count = 0;

esp_err_t gateway_state_init(void)
{
    if (s_state_mutex == NULL) {
        s_state_mutex = xSemaphoreCreateMutex();
        if (s_state_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t gateway_state_set_network(const gateway_network_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_network_state = *state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_get_network(gateway_network_state_t *out_state)
{
    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    *out_state = s_network_state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_set_wifi(const gateway_wifi_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_wifi_state = *state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_get_wifi(gateway_wifi_state_t *out_state)
{
    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    *out_state = s_wifi_state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_set_devices(const zb_device_t *devices, int count)
{
    if (count < 0 || count > MAX_DEVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    if (count > 0 && devices == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    gateway_device_lqi_state_t prev_lqi[MAX_DEVICES];
    int prev_lqi_count = s_device_lqi_count;
    if (prev_lqi_count > 0) {
        memcpy(prev_lqi, s_device_lqi, sizeof(gateway_device_lqi_state_t) * (size_t)prev_lqi_count);
    }

    s_device_count = count;
    s_device_lqi_count = 0;
    memset(s_devices, 0, sizeof(s_devices));
    memset(s_device_lqi, 0, sizeof(s_device_lqi));
    if (count > 0) {
        memcpy(s_devices, devices, sizeof(zb_device_t) * (size_t)count);
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < prev_lqi_count; j++) {
                if (prev_lqi[j].short_addr == s_devices[i].short_addr) {
                    if (s_device_lqi_count >= MAX_DEVICES) {
                        break;
                    }
                    s_device_lqi[s_device_lqi_count] = prev_lqi[j];
                    s_device_lqi[s_device_lqi_count].short_addr = s_devices[i].short_addr;
                    s_device_lqi_count++;
                    break;
                }
            }
        }
    }
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

int gateway_state_get_devices_snapshot(zb_device_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return 0;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    int count = s_device_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, s_devices, sizeof(zb_device_t) * (size_t)count);
    }
    xSemaphoreGive(s_state_mutex);
    return count;
}

esp_err_t gateway_state_update_device_lqi(uint16_t short_addr, int lqi, int rssi, gateway_lqi_source_t source, uint64_t updated_ms)
{
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }
    if (updated_ms == 0) {
        updated_ms = (uint64_t)(esp_timer_get_time() / 1000);
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    int idx = -1;
    for (int i = 0; i < s_device_lqi_count; i++) {
        if (s_device_lqi[i].short_addr == short_addr) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (s_device_lqi_count >= MAX_DEVICES) {
            xSemaphoreGive(s_state_mutex);
            return ESP_ERR_NO_MEM;
        }
        idx = s_device_lqi_count;
        s_device_lqi_count++;
    }

    s_device_lqi[idx].short_addr = short_addr;
    s_device_lqi[idx].lqi = lqi;
    s_device_lqi[idx].rssi = rssi;
    s_device_lqi[idx].source = source;
    s_device_lqi[idx].updated_ms = updated_ms;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

int gateway_state_get_device_lqi_snapshot(gateway_device_lqi_state_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return 0;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    int count = s_device_lqi_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, s_device_lqi, sizeof(gateway_device_lqi_state_t) * (size_t)count);
    }
    xSemaphoreGive(s_state_mutex);
    return count;
}
