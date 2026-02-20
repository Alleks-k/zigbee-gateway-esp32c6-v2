#include "wifi_service.h"
#include "config_service.h"
#include "gateway_status_esp.h"
#include <stdlib.h>
#include <string.h>

struct wifi_service {
    wifi_service_scan_impl_t scan_impl;
    wifi_service_scan_free_impl_t scan_free_impl;
};

esp_err_t wifi_service_create(wifi_service_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_service_handle_t handle = (wifi_service_handle_t)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    *out_handle = handle;
    return ESP_OK;
}

void wifi_service_destroy(wifi_service_handle_t handle)
{
    free(handle);
}

void wifi_service_register_scan_impl(wifi_service_handle_t handle,
                                     wifi_service_scan_impl_t scan_impl,
                                     wifi_service_scan_free_impl_t free_impl)
{
    if (!handle) {
        return;
    }
    handle->scan_impl = scan_impl;
    handle->scan_free_impl = free_impl;
}

esp_err_t wifi_service_scan(wifi_service_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!out_list || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_list = NULL;
    *out_count = 0;

    if (!handle->scan_impl) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return handle->scan_impl(out_list, out_count);
}

void wifi_service_scan_free(wifi_service_handle_t handle, wifi_ap_info_t *list)
{
    if (!handle) {
        return;
    }
    if (!list) {
        return;
    }
    if (handle->scan_free_impl) {
        handle->scan_free_impl(list);
    } else {
        free(list);
    }
}

esp_err_t wifi_service_save_credentials(wifi_service_handle_t handle, const char *ssid, const char *password)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return gateway_status_to_esp_err(config_service_save_wifi_credentials(ssid, password));
}
