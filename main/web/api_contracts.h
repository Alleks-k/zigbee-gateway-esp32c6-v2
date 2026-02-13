#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdint.h>

#define API_WIFI_SSID_MAX_LEN 32
#define API_WIFI_PASSWORD_MAX_LEN 64
#define API_DEVICE_NAME_MAX_LEN 31

typedef struct {
    uint16_t addr;
    uint8_t ep;
    uint8_t cmd;
} api_control_request_t;

typedef struct {
    uint16_t short_addr;
} api_delete_request_t;

typedef struct {
    uint16_t short_addr;
    char name[API_DEVICE_NAME_MAX_LEN + 1];
} api_rename_request_t;

typedef struct {
    char ssid[API_WIFI_SSID_MAX_LEN + 1];
    char password[API_WIFI_PASSWORD_MAX_LEN + 1];
} api_wifi_save_request_t;

esp_err_t api_parse_control_request(httpd_req_t *req, api_control_request_t *out);
esp_err_t api_parse_delete_request(httpd_req_t *req, api_delete_request_t *out);
esp_err_t api_parse_rename_request(httpd_req_t *req, api_rename_request_t *out);
esp_err_t api_parse_wifi_save_request(httpd_req_t *req, api_wifi_save_request_t *out);

esp_err_t api_parse_control_json(const char *json, api_control_request_t *out);
esp_err_t api_parse_delete_json(const char *json, api_delete_request_t *out);
esp_err_t api_parse_rename_json(const char *json, api_rename_request_t *out);
esp_err_t api_parse_wifi_save_json(const char *json, api_wifi_save_request_t *out);
