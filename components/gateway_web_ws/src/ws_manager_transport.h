#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#include <stddef.h>
#include <sys/types.h>

esp_err_t ws_manager_send_frame_to_clients(const char *json, size_t json_len);
int ws_manager_transport_req_to_sockfd(httpd_req_t *req);
esp_err_t ws_manager_transport_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len);
esp_err_t ws_manager_transport_resp_set_status(httpd_req_t *req, const char *status);
esp_err_t ws_manager_transport_resp_send(httpd_req_t *req, const char *buf, ssize_t buf_len);
void ws_manager_transport_close_socket(int fd);
