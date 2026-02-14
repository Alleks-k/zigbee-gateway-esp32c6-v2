#include "api_usecases.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "system_service.h"

static esp_err_t real_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    return zigbee_service_send_on_off(short_addr, endpoint, on_off);
}

static esp_err_t real_wifi_save_credentials(const char *ssid, const char *password)
{
    return wifi_service_save_credentials(ssid, password);
}

static esp_err_t real_schedule_reboot(uint32_t delay_ms)
{
    return system_service_schedule_reboot(delay_ms);
}

static esp_err_t real_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    return system_service_factory_reset_and_reboot(reboot_delay_ms);
}

static const api_service_ops_t s_real_ops = {
    .send_on_off = real_send_on_off,
    .wifi_save_credentials = real_wifi_save_credentials,
    .schedule_reboot = real_schedule_reboot,
    .factory_reset_and_reboot = real_factory_reset_and_reboot,
};

static const api_service_ops_t *s_ops = &s_real_ops;

void api_usecases_set_service_ops(const api_service_ops_t *ops)
{
    s_ops = ops ? ops : &s_real_ops;
}

esp_err_t api_usecase_control(const api_control_request_t *in)
{
    if (!in || !s_ops || !s_ops->send_on_off) {
        return ESP_ERR_INVALID_ARG;
    }
    return s_ops->send_on_off(in->addr, in->ep, in->cmd);
}

esp_err_t api_usecase_wifi_save(const api_wifi_save_request_t *in)
{
    if (!in || !s_ops || !s_ops->wifi_save_credentials || !s_ops->schedule_reboot) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = s_ops->wifi_save_credentials(in->ssid, in->password);
    if (err != ESP_OK) {
        return err;
    }
    return s_ops->schedule_reboot(1000);
}

esp_err_t api_usecase_factory_reset(void)
{
    if (!s_ops || !s_ops->factory_reset_and_reboot) {
        return ESP_ERR_INVALID_ARG;
    }
    return s_ops->factory_reset_and_reboot(1000);
}

esp_err_t api_usecase_get_network_status(zigbee_network_status_t *out_status)
{
    if (!out_status) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_network_status(out_status);
}

int api_usecase_get_devices_snapshot(zb_device_t *out_devices, int max_devices)
{
    return zigbee_service_get_devices_snapshot(out_devices, max_devices);
}

int api_usecase_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!out_neighbors || max_neighbors <= 0) {
        return 0;
    }
    return zigbee_service_get_neighbor_lqi_snapshot(out_neighbors, (size_t)max_neighbors);
}

esp_err_t api_usecase_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                              zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    if (!out_neighbors || max_neighbors <= 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_cached_lqi_snapshot(out_neighbors, (size_t)max_neighbors, out_count, out_source, out_updated_ms);
}

esp_err_t api_usecase_permit_join(uint8_t duration_seconds)
{
    return zigbee_service_permit_join(duration_seconds);
}

esp_err_t api_usecase_delete_device(uint16_t short_addr)
{
    return zigbee_service_delete_device(short_addr);
}

esp_err_t api_usecase_rename_device(uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_rename_device(short_addr, name);
}

esp_err_t api_usecase_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count)
{
    return wifi_service_scan(out_list, out_count);
}

void api_usecase_wifi_scan_free(wifi_ap_info_t *list)
{
    wifi_service_scan_free(list);
}

esp_err_t api_usecase_schedule_reboot(uint32_t delay_ms)
{
    return system_service_schedule_reboot(delay_ms);
}

esp_err_t api_usecase_get_factory_reset_report(system_factory_reset_report_t *out_report)
{
    return system_service_get_last_factory_reset_report(out_report);
}

esp_err_t api_usecase_collect_telemetry(system_telemetry_t *out)
{
    return system_service_collect_telemetry(out);
}
