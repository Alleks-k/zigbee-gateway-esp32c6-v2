#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "gateway_runtime_types.h"

typedef struct wifi_service wifi_service_t;
typedef wifi_service_t *wifi_service_handle_t;

typedef esp_err_t (*wifi_service_scan_impl_t)(wifi_ap_info_t **out_list, size_t *out_count);
typedef void (*wifi_service_scan_free_impl_t)(wifi_ap_info_t *list);

esp_err_t wifi_service_create(wifi_service_handle_t *out_handle);
void wifi_service_destroy(wifi_service_handle_t handle);

void wifi_service_register_scan_impl(wifi_service_handle_t handle,
                                     wifi_service_scan_impl_t scan_impl,
                                     wifi_service_scan_free_impl_t free_impl);

esp_err_t wifi_service_scan(wifi_service_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count);
void wifi_service_scan_free(wifi_service_handle_t handle, wifi_ap_info_t *list);
esp_err_t wifi_service_save_credentials(wifi_service_handle_t handle, const char *ssid, const char *password);
