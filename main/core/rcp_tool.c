/* main/rcp_tool.c */
#include "rcp_tool.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "string.h"
#include "esp_zigbee_gateway.h" // Для RCP_VERSION_MAX_SIZE

#if CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP
#include "esp_rcp_update.h"
#include "esp_spiffs.h"
#endif

#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif

static const char *TAG = "RCP_TOOL";

#if CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP
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

void rcp_init_auto_update(void)
{
    esp_rcp_update_config_t rcp_update_config = ESP_ZB_RCP_UPDATE_CONFIG();
    /* Монтуємо файлову систему з прошивками */
    ESP_ERROR_CHECK(init_spiffs_rcp());
    /* Ініціалізуємо модуль оновлення */
    esp_rcp_update_init(&rcp_update_config);
}
#endif // CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP

void rcp_error_handler(void)
{
    ESP_LOGE(TAG, "RCP error handler triggered");
#if CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP
    esp_zb_gateway_update_rcp();
#endif
    esp_restart();
}

#if CONFIG_OPENTHREAD_SPINEL_ONLY
esp_err_t check_ot_rcp_version(void)
{
    char internal_rcp_version[RCP_VERSION_MAX_SIZE];
    esp_radio_spinel_rcp_version_get(internal_rcp_version, ESP_RADIO_SPINEL_ZIGBEE);
#if CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP
    esp_zb_gateway_board_try_update(internal_rcp_version);
#endif
    return ESP_OK;
}
#endif
