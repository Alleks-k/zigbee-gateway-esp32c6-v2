#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_zigbee_core.h"

/* Зовнішні змінні зі статусом Zigbee */
extern uint16_t pan_id;
extern uint8_t channel;
extern uint16_t short_addr;

/* Прототипи функцій сервера */
esp_err_t web_handler(httpd_req_t *req);
esp_err_t css_handler(httpd_req_t *req);
esp_err_t js_handler(httpd_req_t *req);
esp_err_t api_status_handler(httpd_req_t *req);
esp_err_t api_permit_join_handler(httpd_req_t *req);
esp_err_t api_control_handler(httpd_req_t *req);
esp_err_t api_delete_device_handler(httpd_req_t *req);
esp_err_t api_rename_device_handler(httpd_req_t *req);
esp_err_t favicon_handler(httpd_req_t *req);

void start_web_server(void);
void ws_broadcast_status(void);

#endif