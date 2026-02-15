#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#if CONFIG_GATEWAY_SELF_TEST_APP
typedef struct {
    esp_err_t (*send_frame_async)(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame);
    int (*req_to_sockfd)(httpd_req_t *req);
    esp_err_t (*ws_recv_frame)(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len);
    esp_err_t (*resp_set_status)(httpd_req_t *req, const char *status);
    esp_err_t (*resp_send)(httpd_req_t *req, const char *buf, ssize_t buf_len);
    int (*close_socket)(int fd);
} ws_manager_transport_ops_t;

void ws_manager_set_transport_ops_for_test(const ws_manager_transport_ops_t *ops);
void ws_manager_reset_transport_ops_for_test(void);
#endif

void ws_manager_init(httpd_handle_t server);
esp_err_t ws_handler(httpd_req_t *req);
void ws_broadcast_status(void);
void ws_httpd_close_fn(httpd_handle_t hd, int sockfd);
int ws_manager_get_client_count(void);
