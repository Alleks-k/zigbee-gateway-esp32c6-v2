#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t http_error_send(httpd_req_t *req, int http_status, const char *code, const char *message);
esp_err_t http_error_send_esp(httpd_req_t *req, esp_err_t err, const char *message);
esp_err_t http_success_send(httpd_req_t *req, const char *message);
