#include <string.h>

#include "esp_coexist.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_zigbee_gateway.h"
#include "gateway_events.h"
#include "gateway_status_esp.h"
#include "gateway_zigbee_runtime_internal.h"
#include "zigbee_service.h"
#include <zcl/esp_zigbee_zcl_core.h>

static const char *TAG = "ZIGBEE_RUNTIME";
#define LIVE_LQI_REFRESH_MIN_INTERVAL_US (3 * 1000 * 1000)

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

void refresh_lqi_from_live_event(const char *reason)
{
    int64_t now_us = esp_timer_get_time();
    if ((now_us - s_last_live_lqi_refresh_us) < LIVE_LQI_REFRESH_MIN_INTERVAL_US) {
        return;
    }
    s_last_live_lqi_refresh_us = now_us;

    esp_err_t ret = zigbee_service_refresh_neighbor_lqi_from_table();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Live LQI refresh failed (%s): %s", reason, esp_err_to_name(ret));
        return;
    }
    (void)esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, NULL, 0, 0);
}

void gateway_state_publish(bool zigbee_started, bool factory_new)
{
    if (!s_gateway_state) {
        ESP_LOGW(TAG, "Gateway state handle is not initialized");
        return;
    }

    gateway_network_state_t state = {
        .zigbee_started = zigbee_started,
        .factory_new = factory_new,
        .pan_id = 0,
        .channel = 0,
        .short_addr = 0,
    };

    if (zigbee_started) {
        state.pan_id = esp_zb_get_pan_id();
        state.channel = esp_zb_get_current_channel();
        state.short_addr = esp_zb_get_short_address();
    }

    esp_err_t ret = gateway_status_to_esp_err(gateway_state_set_network(s_gateway_state, &state));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish gateway state: %s", esp_err_to_name(ret));
    }
}

void device_delete_request_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base != GATEWAY_EVENT || event_id != GATEWAY_EVENT_DEVICE_DELETE_REQUEST || event_data == NULL) {
        return;
    }

    gateway_device_delete_request_event_t *evt = (gateway_device_delete_request_event_t *)event_data;
    esp_zb_bdb_open_network(0);
    send_leave_command(evt->short_addr, evt->ieee_addr);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        gateway_state_publish(true, esp_zb_bdb_is_factory_new());
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
        esp_coex_wifi_i154_enable();
#endif
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            bool factory_new = esp_zb_bdb_is_factory_new();
            ESP_LOGI(TAG, "Device started up. Factory new: %d", factory_new);
            gateway_state_publish(true, factory_new);
            if (factory_new) {
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                esp_zb_bdb_open_network(180);
            }
        } else {
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION,
                                   1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            uint16_t pan_id = esp_zb_get_pan_id();
            uint8_t channel = esp_zb_get_current_channel();
            gateway_state_publish(true, esp_zb_bdb_is_factory_new());
            ESP_LOGI(TAG, "Formed network: PAN 0x%04hx, CH %d", pan_id, channel);
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION,
                                   1000);
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        esp_zb_zdo_signal_device_annce_params_t *params =
            (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device joined: 0x%04hx", params->device_short_addr);
        gateway_device_announce_event_t evt = {
            .short_addr = params->device_short_addr,
        };
        memcpy(evt.ieee_addr, params->ieee_addr, sizeof(evt.ieee_addr));
        esp_err_t post_ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, &evt, sizeof(evt), 0);
        if (post_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post DEVICE_ANNOUNCE event: %s", esp_err_to_name(post_ret));
        }
        esp_err_t close_ret = esp_zb_bdb_close_network();
        if (close_ret == ESP_OK) {
            ESP_LOGI(TAG, "Permit join closed after new device join");
        } else {
            ESP_LOGW(TAG, "Failed to close permit join: %s", esp_err_to_name(close_ret));
        }
        refresh_lqi_from_live_event("device_announce");
        break;
    }

    default:
        ESP_LOGI(TAG, "ZDO signal: 0x%x, status: %s", sig_type, esp_err_to_name(err_status));
        break;
    }
}

esp_err_t gateway_zigbee_runtime_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID: {
        esp_zb_zcl_report_attr_message_t *report = (esp_zb_zcl_report_attr_message_t *)message;
        if (report->cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF &&
            report->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            bool state = *(bool *)report->attribute.data.value;
            ESP_LOGI(TAG, "Device 0x%04x report: State is %s", report->src_address.u.short_addr, state ? "ON" : "OFF");
        }
        refresh_lqi_from_live_event("report_attr");
        break;
    }
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        break;
    default:
        break;
    }
    return ret;
}
