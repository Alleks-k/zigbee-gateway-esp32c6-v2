#include "api_contracts.h"
#include "cJSON.h"
#include <string.h>

static bool valid_short_addr(int value)
{
    return value > 0 && value <= 0xFFFF;
}

static esp_err_t parse_control_root(cJSON *root, api_control_request_t *out)
{
    cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
    cJSON *ep_item = cJSON_GetObjectItem(root, "ep");
    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsNumber(addr_item) || !cJSON_IsNumber(ep_item) || !cJSON_IsNumber(cmd_item)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!valid_short_addr(addr_item->valueint)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ep_item->valueint <= 0 || ep_item->valueint > 240) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!(cmd_item->valueint == 0 || cmd_item->valueint == 1)) {
        return ESP_ERR_INVALID_ARG;
    }

    out->addr = (uint16_t)addr_item->valueint;
    out->ep = (uint8_t)ep_item->valueint;
    out->cmd = (uint8_t)cmd_item->valueint;
    return ESP_OK;
}

static esp_err_t parse_delete_root(cJSON *root, api_delete_request_t *out)
{
    cJSON *addr_item = cJSON_GetObjectItem(root, "short_addr");
    if (!cJSON_IsNumber(addr_item)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!valid_short_addr(addr_item->valueint)) {
        return ESP_ERR_INVALID_ARG;
    }
    out->short_addr = (uint16_t)addr_item->valueint;
    return ESP_OK;
}

static esp_err_t parse_rename_root(cJSON *root, api_rename_request_t *out)
{
    cJSON *addr_item = cJSON_GetObjectItem(root, "short_addr");
    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    if (!cJSON_IsNumber(addr_item) || !cJSON_IsString(name_item) || !name_item->valuestring) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!valid_short_addr(addr_item->valueint)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t name_len = strlen(name_item->valuestring);
    if (name_len == 0 || name_len > API_DEVICE_NAME_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    out->short_addr = (uint16_t)addr_item->valueint;
    strlcpy(out->name, name_item->valuestring, sizeof(out->name));
    return ESP_OK;
}

static esp_err_t parse_wifi_save_root(cJSON *root, api_wifi_save_request_t *out)
{
    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_item = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring ||
        !cJSON_IsString(pass_item) || !pass_item->valuestring)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strlen(ssid_item->valuestring);
    size_t pass_len = strlen(pass_item->valuestring);
    if (ssid_len == 0 || ssid_len > API_WIFI_SSID_MAX_LEN ||
        pass_len < 8 || pass_len > API_WIFI_PASSWORD_MAX_LEN)
    {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(out->ssid, ssid_item->valuestring, sizeof(out->ssid));
    strlcpy(out->password, pass_item->valuestring, sizeof(out->password));
    return ESP_OK;
}

static bool valid_job_type(const char *type)
{
    return type &&
           (strcmp(type, "scan") == 0 ||
            strcmp(type, "factory_reset") == 0 ||
            strcmp(type, "reboot") == 0 ||
            strcmp(type, "update") == 0 ||
            strcmp(type, "lqi_refresh") == 0);
}

static esp_err_t parse_job_submit_root(cJSON *root, api_job_submit_request_t *out)
{
    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type_item) || !type_item->valuestring || !valid_job_type(type_item->valuestring)) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(out->type, type_item->valuestring, sizeof(out->type));

    out->reboot_delay_ms = 1000;
    cJSON *delay_item = cJSON_GetObjectItem(root, "reboot_delay_ms");
    if (delay_item != NULL) {
        if (!cJSON_IsNumber(delay_item) || delay_item->valueint < 0 || delay_item->valueint > 60000) {
            return ESP_ERR_INVALID_ARG;
        }
        out->reboot_delay_ms = (uint32_t)delay_item->valueint;
    }
    return ESP_OK;
}

static esp_err_t parse_json_string(const char *json, cJSON **out_root)
{
    if (!json || !out_root) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static esp_err_t parse_json_body(httpd_req_t *req, cJSON **out_root, char *buf, size_t buf_size)
{
    if (!req || !out_root || !buf || buf_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req->content_len <= 0 || (size_t)req->content_len >= buf_size) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t total = 0;
    size_t remaining = (size_t)req->content_len;
    while (remaining > 0) {
        int len = httpd_req_recv(req, buf + total, remaining);
        if (len <= 0) {
            return ESP_FAIL;
        }
        total += (size_t)len;
        remaining -= (size_t)len;
    }
    buf[total] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_root = root;
    return ESP_OK;
}

esp_err_t api_parse_control_request(httpd_req_t *req, api_control_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[128];
    cJSON *root = NULL;
    esp_err_t err = parse_json_body(req, &root, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    err = parse_control_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_delete_request(httpd_req_t *req, api_delete_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[96];
    cJSON *root = NULL;
    esp_err_t err = parse_json_body(req, &root, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    err = parse_delete_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_rename_request(httpd_req_t *req, api_rename_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[192];
    cJSON *root = NULL;
    esp_err_t err = parse_json_body(req, &root, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    err = parse_rename_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_wifi_save_request(httpd_req_t *req, api_wifi_save_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[256];
    cJSON *root = NULL;
    esp_err_t err = parse_json_body(req, &root, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    err = parse_wifi_save_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_job_submit_request(httpd_req_t *req, api_job_submit_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[160];
    cJSON *root = NULL;
    esp_err_t err = parse_json_body(req, &root, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    err = parse_job_submit_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_control_json(const char *json, api_control_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = NULL;
    esp_err_t err = parse_json_string(json, &root);
    if (err != ESP_OK) {
        return err;
    }
    err = parse_control_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_delete_json(const char *json, api_delete_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = NULL;
    esp_err_t err = parse_json_string(json, &root);
    if (err != ESP_OK) {
        return err;
    }
    err = parse_delete_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_rename_json(const char *json, api_rename_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = NULL;
    esp_err_t err = parse_json_string(json, &root);
    if (err != ESP_OK) {
        return err;
    }
    err = parse_rename_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_wifi_save_json(const char *json, api_wifi_save_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = NULL;
    esp_err_t err = parse_json_string(json, &root);
    if (err != ESP_OK) {
        return err;
    }
    err = parse_wifi_save_root(root, out);
    cJSON_Delete(root);
    return err;
}

esp_err_t api_parse_job_submit_json(const char *json, api_job_submit_request_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *root = NULL;
    esp_err_t err = parse_json_string(json, &root);
    if (err != ESP_OK) {
        return err;
    }
    err = parse_job_submit_root(root, out);
    cJSON_Delete(root);
    return err;
}
