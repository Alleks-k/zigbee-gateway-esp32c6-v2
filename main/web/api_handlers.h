#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <stddef.h>

/**
 * @brief Генерує JSON зі статусом шлюзу та пристроїв.
 * Використовується API та WebSocket.
 * @return char* Рядок JSON (потрібно звільнити free())
 */
char* create_status_json(void);
esp_err_t build_status_json_compact(char *out, size_t out_size, size_t *out_len);

/* API Handlers */
esp_err_t api_status_handler(httpd_req_t *req);
esp_err_t api_permit_join_handler(httpd_req_t *req);
esp_err_t api_control_handler(httpd_req_t *req);
esp_err_t api_delete_device_handler(httpd_req_t *req);
esp_err_t api_rename_device_handler(httpd_req_t *req);
esp_err_t api_wifi_scan_handler(httpd_req_t *req);
esp_err_t api_wifi_save_handler(httpd_req_t *req);
esp_err_t api_reboot_handler(httpd_req_t *req);
