#include "state_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static SemaphoreHandle_t s_state_mutex = NULL;
static gateway_network_state_t s_network_state = {0};
static gateway_wifi_state_t s_wifi_state = {0};
static gateway_lqi_cache_entry_t s_lqi_cache[GATEWAY_STATE_LQI_CACHE_CAPACITY];
static int s_lqi_cache_count = 0;

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

esp_err_t gateway_state_update_lqi(uint16_t short_addr, int lqi, int rssi, gateway_lqi_source_t source, uint64_t updated_ms)
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
    for (int i = 0; i < s_lqi_cache_count; i++) {
        if (s_lqi_cache[i].short_addr == short_addr) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (s_lqi_cache_count >= GATEWAY_STATE_LQI_CACHE_CAPACITY) {
            xSemaphoreGive(s_state_mutex);
            return ESP_ERR_NO_MEM;
        }
        idx = s_lqi_cache_count;
        s_lqi_cache_count++;
    }

    s_lqi_cache[idx].short_addr = short_addr;
    s_lqi_cache[idx].lqi = lqi;
    s_lqi_cache[idx].rssi = rssi;
    s_lqi_cache[idx].source = source;
    s_lqi_cache[idx].updated_ms = updated_ms;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

int gateway_state_get_lqi_snapshot(gateway_lqi_cache_entry_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return 0;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    int count = s_lqi_cache_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, s_lqi_cache, sizeof(gateway_lqi_cache_entry_t) * (size_t)count);
    }
    xSemaphoreGive(s_state_mutex);
    return count;
}
