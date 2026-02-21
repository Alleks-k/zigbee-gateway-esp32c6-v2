#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "http_error.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "API_HANDLERS";

static api_usecases_handle_t req_usecases(httpd_req_t *req)
{
    return req ? (api_usecases_handle_t)req->user_ctx : NULL;
}

static esp_err_t send_json_escaped_chunk(httpd_req_t *req, const char *src)
{
    if (!req || !src) {
        return ESP_ERR_INVALID_ARG;
    }

    char chunk[64];
    size_t used = 0;
    for (; *src; src++) {
        unsigned char ch = (unsigned char)*src;
        char escaped[7];
        const char *token = NULL;
        size_t token_len = 0;

        if (ch == '"' || ch == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)ch;
            escaped[2] = '\0';
            token = escaped;
            token_len = 2;
        } else if (ch < 0x20) {
            int written = snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)ch);
            if (written != 6) {
                return ESP_FAIL;
            }
            token = escaped;
            token_len = 6;
        } else {
            escaped[0] = (char)ch;
            escaped[1] = '\0';
            token = escaped;
            token_len = 1;
        }

        if (used + token_len >= sizeof(chunk) - 1) {
            chunk[used] = '\0';
            esp_err_t ret = httpd_resp_sendstr_chunk(req, chunk);
            if (ret != ESP_OK) {
                return ret;
            }
            used = 0;
        }

        memcpy(chunk + used, token, token_len);
        used += token_len;
    }

    if (used > 0) {
        chunk[used] = '\0';
        return httpd_resp_sendstr_chunk(req, chunk);
    }
    return ESP_OK;
}

esp_err_t api_permit_join_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    esp_err_t ret = api_usecase_permit_join(usecases, 60);
    if (ret != ESP_OK) {
        return http_error_send_esp(req, ret, "Failed to open network");
    }
    ESP_LOGI(TAG, "Network opened for 60 seconds via Web API");
    return http_success_send(req, "Network opened for 60 seconds");
}

esp_err_t api_control_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    api_control_request_t in = {0};
    if (api_parse_control_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Missing parameters");
    }

    ESP_LOGI(TAG, "Web Control: addr=0x%04x, ep=%d, cmd=%d", in.addr, in.ep, in.cmd);
    esp_err_t err = api_usecase_control(usecases, &in);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Failed to send command");
    }
    return http_success_send(req, "Command sent");
}

esp_err_t api_delete_device_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    api_delete_request_t in = {0};
    if (api_parse_delete_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_delete_device(usecases, in.short_addr) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Delete failed");
    }
    return http_success_send(req, "Device deleted");
}

esp_err_t api_rename_device_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    api_rename_request_t in = {0};
    if (api_parse_rename_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_rename_device(usecases, in.short_addr, in.name) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Rename failed");
    }
    return http_success_send(req, "Device renamed");
}

esp_err_t api_wifi_scan_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t err = api_usecase_wifi_scan(usecases, &list, &count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed in service: %s", esp_err_to_name(err));
        return http_error_send_esp(req, err, "Scan failed");
    }

    if (count == 0) {
        api_usecase_wifi_scan_free(usecases, list);
        return http_success_send_data_json(req, "[]");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t send_ret = httpd_resp_sendstr_chunk(req, "{\"status\":\"ok\",\"data\":[");
    for (size_t i = 0; send_ret == ESP_OK && i < count; i++) {
        if (i > 0) {
            send_ret = httpd_resp_sendstr_chunk(req, ",");
            if (send_ret != ESP_OK) {
                break;
            }
        }
        send_ret = httpd_resp_sendstr_chunk(req, "{\"ssid\":\"");
        if (send_ret != ESP_OK) {
            break;
        }
        send_ret = send_json_escaped_chunk(req, list[i].ssid);
        if (send_ret != ESP_OK) {
            break;
        }
        char tail[48];
        int written = snprintf(tail, sizeof(tail), "\",\"rssi\":%d,\"auth\":%u}", list[i].rssi, (unsigned)list[i].auth);
        if (written < 0 || (size_t)written >= sizeof(tail)) {
            send_ret = ESP_ERR_NO_MEM;
            break;
        }
        send_ret = httpd_resp_sendstr_chunk(req, tail);
    }

    api_usecase_wifi_scan_free(usecases, list);
    if (send_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stream scan JSON: %s", esp_err_to_name(send_ret));
        return send_ret;
    }
    send_ret = httpd_resp_sendstr_chunk(req, "]}");
    if (send_ret != ESP_OK) {
        return send_ret;
    }
    return httpd_resp_sendstr_chunk(req, NULL);
}

esp_err_t api_reboot_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    if (api_usecase_schedule_reboot(usecases, 1000) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Failed to schedule reboot");
    }
    return http_success_send(req, "Rebooting...");
}

esp_err_t api_factory_reset_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    esp_err_t err = api_usecase_factory_reset(usecases);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Factory reset failed");
    }

    api_factory_reset_report_t report = {0};
    err = api_usecase_get_factory_reset_report(usecases, &report);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Factory reset status unavailable");
    }

    char data_json[352];
    int written = snprintf(data_json, sizeof(data_json),
        "{\"message\":\"Factory reset done. Rebooting...\","
        "\"details\":{\"wifi\":\"%s\",\"devices\":\"%s\",\"zigbee_storage\":\"%s\",\"zigbee_fct\":\"%s\"}}}",
        esp_err_to_name(report.wifi_err),
        esp_err_to_name(report.devices_err),
        esp_err_to_name(report.zigbee_storage_err),
        esp_err_to_name(report.zigbee_fct_err));
    if (written < 0 || (size_t)written >= sizeof(data_json)) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Factory reset response too large");
    }
    esp_err_t send_ret = http_success_send_data_json(req, data_json);
    if (send_ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Factory reset response too large");
    }
    return send_ret;
}

esp_err_t api_wifi_save_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    api_wifi_save_request_t in = {0};
    if (api_parse_wifi_save_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    esp_err_t err = api_usecase_wifi_save(usecases, &in);
    if (err == ESP_OK) {
        return http_success_send(req, "Saved. Restarting...");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return http_error_send_esp(req, err, "Invalid SSID or password");
    }

    ESP_LOGE(TAG, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
    return http_error_send_esp(req, err, "Failed to save Wi-Fi credentials");
}
