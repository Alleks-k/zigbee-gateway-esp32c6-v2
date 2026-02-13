#include "gateway_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static SemaphoreHandle_t s_state_mutex = NULL;
static gateway_network_state_t s_network_state = {0};
static gateway_wifi_state_t s_wifi_state = {0};
static zb_device_t s_devices[MAX_DEVICES];
static int s_device_count = 0;

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
    s_device_count = count;
    if (count > 0) {
        memcpy(s_devices, devices, sizeof(zb_device_t) * (size_t)count);
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
