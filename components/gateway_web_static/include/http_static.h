#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t web_handler(httpd_req_t *req);
esp_err_t css_handler(httpd_req_t *req);
esp_err_t js_handler(httpd_req_t *req);
esp_err_t favicon_handler(httpd_req_t *req);
