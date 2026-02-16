#include "gateway_device_zigbee_facade.h"

#include "zigbee_service.h"

esp_err_t gateway_device_zigbee_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    return zigbee_service_send_on_off(short_addr, endpoint, on_off);
}

esp_err_t gateway_device_zigbee_get_network_status(zigbee_network_status_t *out_status)
{
    return zigbee_service_get_network_status(out_status);
}

int gateway_device_zigbee_get_devices_snapshot(zb_device_t *out_devices, int max_devices)
{
    if (!out_devices || max_devices <= 0) {
        return 0;
    }
    return zigbee_service_get_devices_snapshot(out_devices, (size_t)max_devices);
}

int gateway_device_zigbee_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!out_neighbors || max_neighbors <= 0) {
        return 0;
    }
    return zigbee_service_get_neighbor_lqi_snapshot(out_neighbors, (size_t)max_neighbors);
}

esp_err_t gateway_device_zigbee_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                                      zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    if (!out_neighbors || max_neighbors <= 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_cached_lqi_snapshot(out_neighbors, (size_t)max_neighbors, out_count, out_source, out_updated_ms);
}

esp_err_t gateway_device_zigbee_permit_join(uint8_t duration_seconds)
{
    return zigbee_service_permit_join(duration_seconds);
}

esp_err_t gateway_device_zigbee_delete_device(uint16_t short_addr)
{
    return zigbee_service_delete_device(short_addr);
}

esp_err_t gateway_device_zigbee_rename_device(uint16_t short_addr, const char *name)
{
    return zigbee_service_rename_device(short_addr, name);
}
