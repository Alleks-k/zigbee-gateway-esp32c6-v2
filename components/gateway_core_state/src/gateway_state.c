#include "state_store.h"

#include <stdlib.h>
#include <string.h>

#include "gateway_state_lock.h"

struct gateway_state_store {
    gateway_state_lock_t state_lock;
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

gateway_status_t gateway_state_set_lock_backend(gateway_state_lock_backend_t backend)
{
    return gateway_state_lock_select_backend(backend);
}

gateway_status_t gateway_state_create(gateway_state_handle_t *out_handle)
{
    if (!out_handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    gateway_state_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return GATEWAY_STATUS_NO_MEM;
    }

    *out_handle = handle;
    return GATEWAY_STATUS_OK;
}

void gateway_state_destroy(gateway_state_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (handle->state_lock) {
        gateway_state_lock_destroy(handle->state_lock);
        handle->state_lock = NULL;
    }
    handle->network_state = (gateway_network_state_t){0};
    handle->wifi_state = (gateway_wifi_state_t){0};
    handle->lqi_cache_count = 0;
    memset(handle->lqi_cache, 0, sizeof(handle->lqi_cache));
    free(handle);
}

gateway_status_t gateway_state_init(gateway_state_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (handle->state_lock == NULL) {
        return gateway_state_lock_create(&handle->state_lock);
    }
    return GATEWAY_STATUS_OK;
}

gateway_status_t gateway_state_set_network(gateway_state_handle_t handle, const gateway_network_state_t *state)
{
    if (!handle || !state) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return ret;
    }

    gateway_state_lock_enter(handle->state_lock);
    handle->network_state = *state;
    gateway_state_lock_exit(handle->state_lock);
    return GATEWAY_STATUS_OK;
}

gateway_status_t gateway_state_get_network(gateway_state_handle_t handle, gateway_network_state_t *out_state)
{
    if (!handle || !out_state) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return ret;
    }

    gateway_state_lock_enter(handle->state_lock);
    *out_state = handle->network_state;
    gateway_state_lock_exit(handle->state_lock);
    return GATEWAY_STATUS_OK;
}

gateway_status_t gateway_state_set_wifi(gateway_state_handle_t handle, const gateway_wifi_state_t *state)
{
    if (!handle || !state) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return ret;
    }

    gateway_state_lock_enter(handle->state_lock);
    handle->wifi_state = *state;
    gateway_state_lock_exit(handle->state_lock);
    return GATEWAY_STATUS_OK;
}

gateway_status_t gateway_state_get_wifi(gateway_state_handle_t handle, gateway_wifi_state_t *out_state)
{
    if (!handle || !out_state) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return ret;
    }

    gateway_state_lock_enter(handle->state_lock);
    *out_state = handle->wifi_state;
    gateway_state_lock_exit(handle->state_lock);
    return GATEWAY_STATUS_OK;
}

gateway_status_t gateway_state_update_lqi(gateway_state_handle_t handle,
                                          uint16_t short_addr,
                                          int lqi,
                                          int rssi,
                                          gateway_lqi_source_t source,
                                          uint64_t updated_ms)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return ret;
    }
    if (updated_ms == 0) {
        updated_ms = gateway_state_now_ms();
    }

    gateway_state_lock_enter(handle->state_lock);
    int idx = -1;
    for (int i = 0; i < handle->lqi_cache_count; i++) {
        if (handle->lqi_cache[i].short_addr == short_addr) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (handle->lqi_cache_count >= GATEWAY_STATE_LQI_CACHE_CAPACITY) {
            gateway_state_lock_exit(handle->state_lock);
            return GATEWAY_STATUS_NO_MEM;
        }
        idx = handle->lqi_cache_count;
        handle->lqi_cache_count++;
    }

    handle->lqi_cache[idx].short_addr = short_addr;
    handle->lqi_cache[idx].lqi = lqi;
    handle->lqi_cache[idx].rssi = rssi;
    handle->lqi_cache[idx].source = source;
    handle->lqi_cache[idx].updated_ms = updated_ms;
    gateway_state_lock_exit(handle->state_lock);
    return GATEWAY_STATUS_OK;
}

int gateway_state_get_lqi_snapshot(gateway_state_handle_t handle, gateway_lqi_cache_entry_t *out, size_t max_items)
{
    if (!handle || !out || max_items == 0) {
        return 0;
    }
    gateway_status_t ret = gateway_state_init(handle);
    if (ret != GATEWAY_STATUS_OK) {
        return 0;
    }

    gateway_state_lock_enter(handle->state_lock);
    int count = handle->lqi_cache_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, handle->lqi_cache, sizeof(gateway_lqi_cache_entry_t) * (size_t)count);
    }
    gateway_state_lock_exit(handle->state_lock);
    return count;
}
