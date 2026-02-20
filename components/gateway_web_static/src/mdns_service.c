#include "mdns_service.h"
#include "esp_log.h"
#include "mdns.h"
#include "gateway_hostname_settings.h"

static const char *TAG = "MDNS_SERVICE";

void start_mdns_service(void)
{
    esp_err_t err = mdns_init();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "mDNS already initialized");
    } else if (err) {
        ESP_LOGE(TAG, "mDNS init failed: %d", err);
        return;
    }

    mdns_hostname_set(GATEWAY_MDNS_HOSTNAME);
    mdns_instance_name_set(GATEWAY_MDNS_INSTANCE);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://%s.local", GATEWAY_MDNS_HOSTNAME);
}
