#include "zigbee_service.h"
#include "gateway_state.h"
#include "esp_zigbee_core.h"
#include "nwk/esp_zigbee_nwk.h"

/* Implemented in app/main integration layer */
void send_on_off_command(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
void delete_device(uint16_t short_addr);
void update_device_name(uint16_t short_addr, const char *new_name);

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
    return gateway_state_get_devices_snapshot(out, max_items);
}

int zigbee_service_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }

    gateway_network_state_t state = {0};
    if (gateway_state_get_network(&state) != ESP_OK || !state.zigbee_started) {
        return 0;
    }

    esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    int count = 0;
    while ((size_t)count < max_items) {
        esp_zb_nwk_neighbor_info_t info = {0};
        esp_err_t ret = esp_zb_nwk_get_next_neighbor(&it, &info);
        if (ret != ESP_OK) {
            break;
        }

        out[count].short_addr = info.short_addr;
        out[count].lqi = (int)info.lqi;
        out[count].rssi = (int)info.rssi;
        out[count].relationship = info.relationship;
        out[count].depth = info.depth;
        count++;
    }

    return count;
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
