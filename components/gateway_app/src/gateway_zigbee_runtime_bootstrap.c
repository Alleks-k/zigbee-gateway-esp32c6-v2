#include <fcntl.h>

#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_zigbee_gateway.h"
#include "esp_zigbee_type.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gateway_events.h"
#include "gateway_zigbee_runtime.h"
#include "gateway_zigbee_runtime_internal.h"
#include "rcp_tool.h"
#include "zigbee_service.h"
#include <zcl/esp_zigbee_zcl_core.h>

#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif

static const char *TAG = "ZIGBEE_RUNTIME";

esp_event_handler_instance_t s_delete_req_handler = NULL;
int64_t s_last_live_lqi_refresh_us = 0;
device_service_handle_t s_device_service = NULL;
gateway_state_handle_t s_gateway_state = NULL;
zigbee_service_handle_t s_zigbee_service = NULL;

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

    esp_zb_core_action_handler_register(gateway_zigbee_runtime_action_handler);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
    vTaskDelete(NULL);
}

esp_err_t gateway_zigbee_runtime_prepare(const gateway_runtime_context_t *ctx)
{
    if (!ctx || !ctx->device_service || !ctx->gateway_state) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_zigbee_service || s_delete_req_handler || s_device_service || s_gateway_state) {
        gateway_zigbee_runtime_teardown();
    }
    s_device_service = ctx->device_service;
    s_gateway_state = ctx->gateway_state;

    zigbee_service_init_params_t params = {
        .device_service = s_device_service,
        .gateway_state = s_gateway_state,
        .runtime_ops = gateway_zigbee_runtime_get_ops(),
    };

    esp_err_t ret = zigbee_service_create(&params, &s_zigbee_service);
    if (ret != ESP_OK) {
        return ret;
    }
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

zigbee_service_handle_t gateway_zigbee_runtime_get_service_handle(void)
{
    return s_zigbee_service;
}

void gateway_zigbee_runtime_teardown(void)
{
    if (s_delete_req_handler) {
        (void)esp_event_handler_instance_unregister(
            GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_DELETE_REQUEST, s_delete_req_handler);
        s_delete_req_handler = NULL;
    }

    if (s_zigbee_service) {
        zigbee_service_destroy(s_zigbee_service);
        s_zigbee_service = NULL;
    }

    s_device_service = NULL;
    s_gateway_state = NULL;
    s_last_live_lqi_refresh_us = 0;
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
        ESP_LOGE(TAG, "Failed to create Zigbee task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
