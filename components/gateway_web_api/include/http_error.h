#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_http_server.h"

typedef bool (*http_error_map_provider_t)(esp_err_t err, int *out_http_status, const char **out_error_code);

bool http_error_map_provider_hook(esp_err_t err, int *out_http_status, const char **out_error_code);
esp_err_t http_error_send(httpd_req_t *req, int http_status, const char *code, const char *message);
esp_err_t http_error_send_esp(httpd_req_t *req, esp_err_t err, const char *message);
esp_err_t http_success_send(httpd_req_t *req, const char *message);
esp_err_t http_success_send_data_json(httpd_req_t *req, const char *data_json);
