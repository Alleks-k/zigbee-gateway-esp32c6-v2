#include "zigbee_service.h"
#include "esp_zigbee_gateway.h"
#include "gateway_state.h"

esp_err_t zigbee_service_get_network_status(zigbee_network_status_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_network_state_t state = {0};
    esp_err_t ret = gateway_state_get_network(&state);
    if (ret != ESP_OK) {
        return ret;
    }
    out->pan_id = state.pan_id;
    out->channel = state.channel;
    out->short_addr = state.short_addr;
    return ESP_OK;
}

esp_err_t zigbee_service_permit_join(uint16_t seconds)
{
    esp_zb_bdb_open_network(seconds);
    return ESP_OK;
}

esp_err_t zigbee_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    send_on_off_command(short_addr, endpoint, on_off);
    return ESP_OK;
}

int zigbee_service_get_devices_snapshot(zb_device_t *out, size_t max_items)
{
    return device_manager_get_snapshot(out, max_items);
}

esp_err_t zigbee_service_delete_device(uint16_t short_addr)
{
    delete_device(short_addr);
    return ESP_OK;
}

esp_err_t zigbee_service_rename_device(uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    update_device_name(short_addr, name);
    return ESP_OK;
}
