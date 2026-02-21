#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_runtime_types.h"
#include "device_service.h"

struct gateway_state_store;

typedef struct {
    esp_err_t (*send_on_off)(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
    esp_err_t (*delete_device)(uint16_t short_addr);
    esp_err_t (*rename_device)(uint16_t short_addr, const char *name);
} zigbee_service_runtime_ops_t;

typedef struct zigbee_service zigbee_service_t;
typedef zigbee_service_t *zigbee_service_handle_t;

typedef struct {
    device_service_handle_t device_service;
    struct gateway_state_store *gateway_state;
    const zigbee_service_runtime_ops_t *runtime_ops;
} zigbee_service_init_params_t;

esp_err_t zigbee_service_create(const zigbee_service_init_params_t *params, zigbee_service_handle_t *out_handle);
void zigbee_service_destroy(zigbee_service_handle_t handle);

esp_err_t zigbee_service_get_network_status(zigbee_service_handle_t handle, zigbee_network_status_t *out);
esp_err_t zigbee_service_permit_join(zigbee_service_handle_t handle, uint16_t seconds);
esp_err_t zigbee_service_send_on_off(zigbee_service_handle_t handle, uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
int zigbee_service_get_devices_snapshot(zigbee_service_handle_t handle, zb_device_t *out, size_t max_items);
int zigbee_service_get_neighbor_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out, size_t max_items);
esp_err_t zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out,
                                                       size_t max_items, int *out_count);
esp_err_t zigbee_service_refresh_neighbor_lqi_from_table(zigbee_service_handle_t handle);
esp_err_t zigbee_service_get_cached_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out,
                                                 size_t max_items, int *out_count,
                                                 zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms);
esp_err_t zigbee_service_delete_device(zigbee_service_handle_t handle, uint16_t short_addr);
esp_err_t zigbee_service_rename_device(zigbee_service_handle_t handle, uint16_t short_addr, const char *name);
