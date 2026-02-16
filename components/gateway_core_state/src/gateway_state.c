#include "state_store.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct gateway_state_store {
    SemaphoreHandle_t state_mutex;
    gateway_network_state_t network_state;
    gateway_wifi_state_t wifi_state;
    gateway_lqi_cache_entry_t lqi_cache[GATEWAY_STATE_LQI_CACHE_CAPACITY];
    int lqi_cache_count;
};

static gateway_state_now_ms_provider_t s_now_ms_provider = NULL;
static uint64_t s_fallback_now_ms = 0;

static uint64_t gateway_state_now_ms(void)
{
    if (s_now_ms_provider) {
        return s_now_ms_provider();
    }
    return ++s_fallback_now_ms;
}

void gateway_state_set_now_ms_provider(gateway_state_now_ms_provider_t provider)
{
    s_now_ms_provider = provider;
}

esp_err_t gateway_state_create(gateway_state_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_state_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    *out_handle = handle;
    return ESP_OK;
}

void gateway_state_destroy(gateway_state_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (handle->state_mutex) {
        vSemaphoreDelete(handle->state_mutex);
        handle->state_mutex = NULL;
    }
    handle->network_state = (gateway_network_state_t){0};
    handle->wifi_state = (gateway_wifi_state_t){0};
    handle->lqi_cache_count = 0;
    memset(handle->lqi_cache, 0, sizeof(handle->lqi_cache));
    free(handle);
}

esp_err_t gateway_state_init(gateway_state_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state_mutex == NULL) {
        handle->state_mutex = xSemaphoreCreateMutex();
        if (handle->state_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t gateway_state_set_network(gateway_state_handle_t handle, const gateway_network_state_t *state)
{
    if (!handle || !state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    handle->network_state = *state;
    xSemaphoreGive(handle->state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_get_network(gateway_state_handle_t handle, gateway_network_state_t *out_state)
{
    if (!handle || !out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    *out_state = handle->network_state;
    xSemaphoreGive(handle->state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_set_wifi(gateway_state_handle_t handle, const gateway_wifi_state_t *state)
{
    if (!handle || !state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    handle->wifi_state = *state;
    xSemaphoreGive(handle->state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_get_wifi(gateway_state_handle_t handle, gateway_wifi_state_t *out_state)
{
    if (!handle || !out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    *out_state = handle->wifi_state;
    xSemaphoreGive(handle->state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_update_lqi(gateway_state_handle_t handle,
                                   uint16_t short_addr,
                                   int lqi,
                                   int rssi,
                                   gateway_lqi_source_t source,
                                   uint64_t updated_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    if (updated_ms == 0) {
        updated_ms = gateway_state_now_ms();
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    int idx = -1;
    for (int i = 0; i < handle->lqi_cache_count; i++) {
        if (handle->lqi_cache[i].short_addr == short_addr) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (handle->lqi_cache_count >= GATEWAY_STATE_LQI_CACHE_CAPACITY) {
            xSemaphoreGive(handle->state_mutex);
            return ESP_ERR_NO_MEM;
        }
        idx = handle->lqi_cache_count;
        handle->lqi_cache_count++;
    }

    handle->lqi_cache[idx].short_addr = short_addr;
    handle->lqi_cache[idx].lqi = lqi;
    handle->lqi_cache[idx].rssi = rssi;
    handle->lqi_cache[idx].source = source;
    handle->lqi_cache[idx].updated_ms = updated_ms;
    xSemaphoreGive(handle->state_mutex);
    return ESP_OK;
}

int gateway_state_get_lqi_snapshot(gateway_state_handle_t handle, gateway_lqi_cache_entry_t *out, size_t max_items)
{
    if (!handle || !out || max_items == 0) {
        return 0;
    }
    esp_err_t ret = gateway_state_init(handle);
    if (ret != ESP_OK) {
        return 0;
    }

    xSemaphoreTake(handle->state_mutex, portMAX_DELAY);
    int count = handle->lqi_cache_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, handle->lqi_cache, sizeof(gateway_lqi_cache_entry_t) * (size_t)count);
    }
    xSemaphoreGive(handle->state_mutex);
    return count;
}
