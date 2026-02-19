#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <sys/types.h>

typedef struct ws_manager_ctx *ws_manager_handle_t;

#if CONFIG_GATEWAY_SELF_TEST_APP
typedef struct {
    esp_err_t (*send_frame_async)(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame);
    int (*req_to_sockfd)(httpd_req_t *req);
    esp_err_t (*ws_recv_frame)(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len);
    esp_err_t (*resp_set_status)(httpd_req_t *req, const char *status);
    esp_err_t (*resp_send)(httpd_req_t *req, const char *buf, ssize_t buf_len);
    int (*close_socket)(int fd);
} ws_manager_transport_ops_t;

void ws_manager_set_transport_ops_for_test_with_handle(ws_manager_handle_t handle, const ws_manager_transport_ops_t *ops);
void ws_manager_reset_transport_ops_for_test_with_handle(ws_manager_handle_t handle);
#endif

esp_err_t ws_manager_create(ws_manager_handle_t *out_handle);
void ws_manager_destroy(ws_manager_handle_t handle);
void ws_manager_init_with_handle(ws_manager_handle_t handle, httpd_handle_t server);
esp_err_t ws_handler_with_handle(ws_manager_handle_t handle, httpd_req_t *req);
void ws_broadcast_status_with_handle(ws_manager_handle_t handle);
void ws_httpd_close_fn_with_handle(ws_manager_handle_t handle, httpd_handle_t hd, int sockfd);
int ws_manager_get_client_count_with_handle(ws_manager_handle_t handle);
