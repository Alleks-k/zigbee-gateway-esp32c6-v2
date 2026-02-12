#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

void ws_manager_init(httpd_handle_t server);
esp_err_t ws_handler(httpd_req_t *req);
void ws_broadcast_status(void);
void ws_httpd_close_fn(httpd_handle_t hd, int sockfd);
