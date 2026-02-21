#include "api_usecases_internal.h"

esp_err_t api_usecase_control(api_usecases_handle_t handle, const api_control_request_t *in)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK || !in) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->service_ops) {
        if (!handle->service_ops->send_on_off) {
            return ESP_ERR_INVALID_ARG;
        }
        return handle->service_ops->send_on_off(in->addr, in->ep, in->cmd);
    }

    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_send_on_off(handle->zigbee_service, in->addr, in->ep, in->cmd);
}

esp_err_t api_usecase_get_network_status(api_usecases_handle_t handle, zigbee_network_status_t *out_status)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK || !out_status) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_get_network_status(handle->zigbee_service, out_status);
}

int api_usecase_get_devices_snapshot(api_usecases_handle_t handle, zb_device_t *out_devices, int max_devices)
{
    if (!handle) {
        return 0;
    }
    if (api_usecases_require_zigbee(handle) != ESP_OK) {
        return 0;
    }
    return gateway_device_zigbee_get_devices_snapshot(handle->zigbee_service, out_devices, max_devices);
}

int api_usecase_get_neighbor_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!handle) {
        return 0;
    }
    if (api_usecases_require_zigbee(handle) != ESP_OK) {
        return 0;
    }
    return gateway_device_zigbee_get_neighbor_lqi_snapshot(handle->zigbee_service, out_neighbors, max_neighbors);
}

esp_err_t api_usecase_get_cached_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors,
                                              int max_neighbors, int *out_count, zigbee_lqi_source_t *out_source,
                                              uint64_t *out_updated_ms)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_get_cached_lqi_snapshot(handle->zigbee_service, out_neighbors, max_neighbors, out_count,
                                                         out_source, out_updated_ms);
}

esp_err_t api_usecase_permit_join(api_usecases_handle_t handle, uint8_t duration_seconds)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_permit_join(handle->zigbee_service, duration_seconds);
}

esp_err_t api_usecase_delete_device(api_usecases_handle_t handle, uint16_t short_addr)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_delete_device(handle->zigbee_service, short_addr);
}

esp_err_t api_usecase_rename_device(api_usecases_handle_t handle, uint16_t short_addr, const char *name)
{
    esp_err_t ret = api_usecases_require_handle(handle);
    if (ret != ESP_OK || !name) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = api_usecases_require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_rename_device(handle->zigbee_service, short_addr, name);
}
