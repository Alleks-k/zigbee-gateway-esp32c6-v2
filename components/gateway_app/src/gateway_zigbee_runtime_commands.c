#include <string.h>

#include "esp_log.h"
#include "esp_zigbee_gateway.h"
#include "gateway_zigbee_runtime_internal.h"

static const char *TAG = "ZIGBEE_RUNTIME";

static esp_err_t zigbee_runtime_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    send_on_off_command(short_addr, endpoint, on_off);
    return ESP_OK;
}

static esp_err_t zigbee_runtime_delete_device(uint16_t short_addr)
{
    if (!s_device_service) {
        return ESP_ERR_INVALID_STATE;
    }
    device_service_delete(s_device_service, short_addr);
    return ESP_OK;
}

static esp_err_t zigbee_runtime_rename_device(uint16_t short_addr, const char *new_name)
{
    if (!new_name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_device_service) {
        return ESP_ERR_INVALID_STATE;
    }
    device_service_update_name(s_device_service, short_addr, new_name);
    return ESP_OK;
}

static const zigbee_service_runtime_ops_t s_zigbee_runtime_ops = {
    .send_on_off = zigbee_runtime_send_on_off,
    .delete_device = zigbee_runtime_delete_device,
    .rename_device = zigbee_runtime_rename_device,
};

const zigbee_service_runtime_ops_t *gateway_zigbee_runtime_get_ops(void)
{
    return &s_zigbee_runtime_ops;
}

void send_on_off_command(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    esp_zb_zcl_on_off_cmd_t cmd_req = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = ESP_ZB_GATEWAY_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .on_off_cmd_id = on_off ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
    };
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
}

void send_leave_command(uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr)
{
    esp_zb_zdo_mgmt_leave_req_param_t leave_req;
    memset(&leave_req, 0, sizeof(esp_zb_zdo_mgmt_leave_req_param_t));

    leave_req.dst_nwk_addr = short_addr;
    memcpy(leave_req.device_address, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    leave_req.remove_children = false;
    leave_req.rejoin = false;

    ESP_LOGI(TAG, "Sending Leave Request to 0x%04x", short_addr);
    esp_zb_zdo_device_leave_req(&leave_req, NULL, NULL);
}
