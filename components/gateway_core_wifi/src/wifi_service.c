#include "wifi_service.h"
#include "config_service.h"
#include <stdlib.h>
#include <string.h>

static wifi_service_scan_impl_t s_scan_impl = NULL;
static wifi_service_scan_free_impl_t s_scan_free_impl = NULL;

void wifi_service_register_scan_impl(wifi_service_scan_impl_t scan_impl, wifi_service_scan_free_impl_t free_impl)
{
    s_scan_impl = scan_impl;
    s_scan_free_impl = free_impl;
}

esp_err_t wifi_service_scan(wifi_ap_info_t **out_list, size_t *out_count)
{
    if (!out_list || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_list = NULL;
    *out_count = 0;

    if (!s_scan_impl) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return s_scan_impl(out_list, out_count);
}

void wifi_service_scan_free(wifi_ap_info_t *list)
{
    if (!list) {
        return;
    }
    if (s_scan_free_impl) {
        s_scan_free_impl(list);
    } else {
        free(list);
    }
}

esp_err_t wifi_service_save_credentials(const char *ssid, const char *password)
{
    return config_service_save_wifi_credentials(ssid, password);
}
