
#include <fcntl.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_eventfd.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_zigbee_type.h"
#include "nvs_flash.h"
#include "wifi_init.h"
#include "esp_rcp_update.h"
#include "esp_coexist.h"
#include "esp_zigbee_gateway.h"
#include "esp_http_server.h"
#include "web_server.h" 
#include "mdns.h"
#include <zcl/esp_zigbee_zcl_core.h>

#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "driver/usb_serial_jtag.h"

#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif

static const char *TAG = "ESP_ZB_GATEWAY";

/* Глобальні дані для статусу мережі */
uint16_t pan_id = 0;
uint8_t channel = 0;
uint16_t short_addr = 0;

/* Production configuration app data */
typedef struct app_production_config_s {
    uint16_t version;
    uint16_t manuf_code;
    char manuf_name[16];
} app_production_config_t;

/* mDNS сервіс */
void start_mdns_service(void)
{
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "mDNS Init failed: %d", err);
        return;
    }
    mdns_hostname_set("zigbee-gw");
    mdns_instance_name_set("ESP32C6 Zigbee Gateway");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://zigbee-gw.local");
}

/* Консоль */
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
esp_err_t esp_zb_gateway_console_init(void)
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

/* Керування RCP (оновлення прошивки копроцесора) */
#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
static void esp_zb_gateway_update_rcp(void)
{
    esp_zb_rcp_deinit();
    if (esp_rcp_update() != ESP_OK) {
        esp_rcp_mark_image_verified(false);
    }
    esp_restart();
}

static void esp_zb_gateway_board_try_update(const char *rcp_version_str)
{
    char version_str[RCP_VERSION_MAX_SIZE];
    if (esp_rcp_load_version_in_storage(version_str, sizeof(version_str)) == ESP_OK) {
        if (strcmp(version_str, rcp_version_str)) {
            ESP_LOGI(TAG, "*** RCP Version mismatch, updating... ***");
            esp_zb_gateway_update_rcp();
        } else {
            esp_rcp_mark_image_verified(true);
        }
    } else {
        esp_rcp_mark_image_verified(false);
        esp_restart();
    }
}

static esp_err_t init_spiffs_rcp(void)
{
    esp_vfs_spiffs_conf_t rcp_fw_conf = {
        .base_path = "/rcp_fw", .partition_label = "rcp_fw", .max_files = 10, .format_if_mount_failed = false
    };
    return esp_vfs_spiffs_register(&rcp_fw_conf);
}
#endif

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

/* Команда On/Off через Zigbee */
void send_on_off_command(uint16_t short_addr, uint8_t endpoint, uint8_t on_off) {
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

/* Обробник сигналів Zigbee Stack */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE
        esp_coex_wifi_i154_enable();
#endif
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up. Factory new: %d", esp_zb_bdb_is_factory_new());
            if (esp_zb_bdb_is_factory_new()) {
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                esp_zb_bdb_open_network(180);
                pan_id = esp_zb_get_pan_id();
                channel = esp_zb_get_current_channel();
                short_addr = esp_zb_get_short_address();
            }
        } else {
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            pan_id = esp_zb_get_pan_id();
            channel = esp_zb_get_current_channel();
            short_addr = esp_zb_get_short_address();
            ESP_LOGI(TAG, "Formed network: PAN 0x%04hx, CH %d", pan_id, channel);
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        esp_zb_zdo_signal_device_annce_params_t *params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device: 0x%04hx", params->device_short_addr);
        add_device(params->device_short_addr);
        break;
    }

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            uint8_t status = *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p);
            ESP_LOGI(TAG, "Permit Join status: %d", status);
        }
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: 0x%x, status: %s", sig_type, esp_err_to_name(err_status));
        break;
    }
}

void rcp_error_handler(void)
{
#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    esp_zb_gateway_update_rcp();
#endif
    esp_restart();
}

#if CONFIG_OPENTHREAD_SPINEL_ONLY
static esp_err_t check_ot_rcp_version(void)
{
    char internal_rcp_version[RCP_VERSION_MAX_SIZE];
    esp_radio_spinel_rcp_version_get(internal_rcp_version, ESP_RADIO_SPINEL_ZIGBEE);
#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    esp_zb_gateway_board_try_update(internal_rcp_version);
#endif
    return ESP_OK;
}
#endif

/* Основне завдання Zigbee */
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
    esp_zb_ep_list_add_gateway_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* 1. Базова ініціалізація системних ресурсів */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 2. Підключення до Wi-Fi (блокує виконання до отримання IP) */
    ESP_ERROR_CHECK(wifi_init_sta_and_wait());

    /* 3. Ініціалізація сховища для Web UI */
    esp_vfs_spiffs_conf_t www_conf = {
        .base_path = "/www", .partition_label = "www", .max_files = 5, .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&www_conf));

    /* 4. Запуск сервісів (доступні відразу після Wi-Fi) */
    start_mdns_service();
    start_web_server();

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_zb_gateway_console_init();
#endif

    /* 5. Налаштування платформи Zigbee (Тільки після готовності мережі) */
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    esp_rcp_update_config_t rcp_update_config = ESP_ZB_RCP_UPDATE_CONFIG();
    init_spiffs_rcp();
    esp_rcp_update_init(&rcp_update_config);
#endif

    /* 6. Запуск основного Zigbee завдання */
    xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);
}