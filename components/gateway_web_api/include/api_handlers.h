#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <stddef.h>

/* API Handlers */
esp_err_t api_status_handler(httpd_req_t *req);
esp_err_t api_lqi_handler(httpd_req_t *req);
esp_err_t api_permit_join_handler(httpd_req_t *req);
esp_err_t api_control_handler(httpd_req_t *req);
esp_err_t api_delete_device_handler(httpd_req_t *req);
esp_err_t api_rename_device_handler(httpd_req_t *req);
esp_err_t api_wifi_scan_handler(httpd_req_t *req);
esp_err_t api_wifi_save_handler(httpd_req_t *req);
esp_err_t api_reboot_handler(httpd_req_t *req);
esp_err_t api_factory_reset_handler(httpd_req_t *req);
esp_err_t api_health_handler(httpd_req_t *req);
esp_err_t api_jobs_submit_handler(httpd_req_t *req);
esp_err_t api_jobs_get_handler(httpd_req_t *req);
