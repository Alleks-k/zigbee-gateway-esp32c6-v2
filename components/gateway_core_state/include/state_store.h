#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gateway_runtime_types.h"
#include "gateway_status.h"

typedef struct gateway_state_store gateway_state_store_t;
typedef gateway_state_store_t *gateway_state_handle_t;
typedef uint64_t (*gateway_state_now_ms_provider_t)(void);

gateway_status_t gateway_state_create(gateway_state_handle_t *out_handle);
void gateway_state_destroy(gateway_state_handle_t handle);
void gateway_state_set_now_ms_provider(gateway_state_now_ms_provider_t provider);
gateway_status_t gateway_state_init(gateway_state_handle_t handle);
gateway_status_t gateway_state_set_network(gateway_state_handle_t handle, const gateway_network_state_t *state);
gateway_status_t gateway_state_get_network(gateway_state_handle_t handle, gateway_network_state_t *out_state);
gateway_status_t gateway_state_set_wifi(gateway_state_handle_t handle, const gateway_wifi_state_t *state);
gateway_status_t gateway_state_get_wifi(gateway_state_handle_t handle, gateway_wifi_state_t *out_state);
gateway_status_t gateway_state_update_lqi(gateway_state_handle_t handle,
                                          uint16_t short_addr,
                                          int lqi,
                                          int rssi,
                                          gateway_lqi_source_t source,
                                          uint64_t updated_ms);
int gateway_state_get_lqi_snapshot(gateway_state_handle_t handle, gateway_lqi_cache_entry_t *out, size_t max_items);
