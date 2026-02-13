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
