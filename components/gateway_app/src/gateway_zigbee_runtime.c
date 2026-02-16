#include <fcntl.h>
#include <string.h>

#include "device_service.h"
#include "driver/usb_serial_jtag.h"
#include "esp_coexist.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_zigbee_gateway.h"
#include "esp_zigbee_type.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gateway_events.h"
#include "state_store.h"
#include "gateway_zigbee_runtime.h"
#include "rcp_tool.h"
#include "zigbee_service.h"
#include <zcl/esp_zigbee_zcl_core.h>

#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif

static const char *TAG = "ZIGBEE_RUNTIME";
static esp_event_handler_instance_t s_delete_req_handler = NULL;
static int64_t s_last_live_lqi_refresh_us = 0;
static device_service_handle_t s_device_service = NULL;
static gateway_state_handle_t s_gateway_state = NULL;
#define LIVE_LQI_REFRESH_MIN_INTERVAL_US (3 * 1000 * 1000)

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
static esp_err_t esp_zb_gateway_console_init(void)
{
    esp_err_t ret = ESP_OK;
    setvbuf(stdin, NULL, _IONBF, 0);
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
    fcntl(fileno(stdout), F_SETFL, O_NONBLOCK);
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    esp_vfs_usb_serial_jtag_use_driver();
    esp_vfs_dev_uart_register();
    return ret;
}
#endif

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

static void refresh_lqi_from_live_event(const char *reason)
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

static void gateway_state_publish(bool zigbee_started, bool factory_new)
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

    esp_err_t ret = gateway_state_set_network(s_gateway_state, &state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish gateway state: %s", esp_err_to_name(ret));
    }
}

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

static void device_delete_request_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
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

static void esp_zb_task(void *pvParameters)
{
#if CONFIG_OPENTHREAD_SPINEL_ONLY
    esp_radio_spinel_register_rcp_failure_handler(rcp_error_handler, ESP_RADIO_SPINEL_ZIGBEE);
#endif
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
#if CONFIG_OPENTHREAD_SPINEL_ONLY
    check_ot_rcp_version();
#endif
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ESP_ZB_GATEWAY_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
    };

    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, esp_zb_on_off_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_ep_list_add_gateway_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);

    esp_zb_core_action_handler_register(zb_action_handler);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
    vTaskDelete(NULL);
}

esp_err_t gateway_zigbee_runtime_prepare(void)
{
    esp_err_t ret = device_service_get_default(&s_device_service);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gateway_state_get_default(&s_gateway_state);
    if (ret != ESP_OK) {
        return ret;
    }

    zigbee_service_set_runtime_ops(&s_zigbee_runtime_ops);
    gateway_state_publish(false, false);

    if (s_delete_req_handler == NULL) {
        return esp_event_handler_instance_register(
            GATEWAY_EVENT,
            GATEWAY_EVENT_DEVICE_DELETE_REQUEST,
            device_delete_request_event_handler,
            NULL,
            &s_delete_req_handler);
    }
    return ESP_OK;
}

esp_err_t gateway_zigbee_runtime_start(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    (void)esp_zb_gateway_console_init();
#endif

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    esp_err_t err = esp_zb_platform_config(&config);
    if (err != ESP_OK) {
        return err;
    }

#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    rcp_init_auto_update();
#endif

    BaseType_t task_ok = xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);
    if (task_ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
