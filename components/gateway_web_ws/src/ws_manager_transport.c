#include "ws_manager_transport.h"

#include "error_ring.h"
#include "ws_manager_internal.h"
#include "ws_manager_state.h"

#include "esp_log.h"

#include <string.h>
#include <unistd.h>

static const char *TAG = "WS_TRANSPORT";

esp_err_t ws_manager_transport_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame);

#if CONFIG_GATEWAY_SELF_TEST_APP
static esp_err_t ws_default_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame)
{
    return httpd_ws_send_frame_async(hd, fd, frame);
}

static int ws_default_req_to_sockfd(httpd_req_t *req)
{
    return httpd_req_to_sockfd(req);
}

static esp_err_t ws_default_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len)
{
    return httpd_ws_recv_frame(req, pkt, max_len);
}

static esp_err_t ws_default_resp_set_status(httpd_req_t *req, const char *status)
{
    return httpd_resp_set_status(req, status);
}

static esp_err_t ws_default_resp_send(httpd_req_t *req, const char *buf, ssize_t buf_len)
{
    return httpd_resp_send(req, buf, buf_len);
}

static int ws_default_close_socket(int fd)
{
    return close(fd);
}

ws_manager_transport_ops_t s_ws_transport_ops = {
    .send_frame_async = ws_default_send_frame_async,
    .req_to_sockfd = ws_default_req_to_sockfd,
    .ws_recv_frame = ws_default_recv_frame,
    .resp_set_status = ws_default_resp_set_status,
    .resp_send = ws_default_resp_send,
    .close_socket = ws_default_close_socket,
};
#endif

esp_err_t ws_manager_send_frame_to_clients(const char *json, size_t json_len)
{
    if (!json || json_len == 0 || !s_server) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)json;
    ws_pkt.len = json_len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            esp_err_t ret = ws_manager_transport_send_frame_async(s_server, ws_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "WS send failed (%s), removing client %d", esp_err_to_name(ret), ws_fds[i]);
                gateway_error_ring_add("ws", (int32_t)ret, "send_frame_async failed");
                ws_manager_inc_dropped_frames();
                ws_fds[i] = -1;
            }
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return ESP_OK;
}

esp_err_t ws_manager_transport_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.send_frame_async) {
        return s_ws_transport_ops.send_frame_async(hd, fd, frame);
    }
    return ESP_ERR_INVALID_STATE;
#else
    return httpd_ws_send_frame_async(hd, fd, frame);
#endif
}

int ws_manager_transport_req_to_sockfd(httpd_req_t *req)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.req_to_sockfd) {
        return s_ws_transport_ops.req_to_sockfd(req);
    }
    return -1;
#else
    return httpd_req_to_sockfd(req);
#endif
}

esp_err_t ws_manager_transport_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.ws_recv_frame) {
        return s_ws_transport_ops.ws_recv_frame(req, pkt, max_len);
    }
    return ESP_ERR_INVALID_STATE;
#else
    return httpd_ws_recv_frame(req, pkt, max_len);
#endif
}

esp_err_t ws_manager_transport_resp_set_status(httpd_req_t *req, const char *status)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.resp_set_status) {
        return s_ws_transport_ops.resp_set_status(req, status);
    }
    return ESP_ERR_INVALID_STATE;
#else
    return httpd_resp_set_status(req, status);
#endif
}

esp_err_t ws_manager_transport_resp_send(httpd_req_t *req, const char *buf, ssize_t buf_len)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.resp_send) {
        return s_ws_transport_ops.resp_send(req, buf, buf_len);
    }
    return ESP_ERR_INVALID_STATE;
#else
    return httpd_resp_send(req, buf, buf_len);
#endif
}

void ws_manager_transport_close_socket(int fd)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_ws_transport_ops.close_socket) {
        (void)s_ws_transport_ops.close_socket(fd);
        return;
    }
#endif
    (void)close(fd);
}

#if CONFIG_GATEWAY_SELF_TEST_APP
void ws_manager_set_transport_ops_for_test(const ws_manager_transport_ops_t *ops)
{
    if (!ops) {
        return;
    }
    s_ws_transport_ops = *ops;
}

void ws_manager_reset_transport_ops_for_test(void)
{
    s_ws_transport_ops.send_frame_async = ws_default_send_frame_async;
    s_ws_transport_ops.req_to_sockfd = ws_default_req_to_sockfd;
    s_ws_transport_ops.ws_recv_frame = ws_default_recv_frame;
    s_ws_transport_ops.resp_set_status = ws_default_resp_set_status;
    s_ws_transport_ops.resp_send = ws_default_resp_send;
    s_ws_transport_ops.close_socket = ws_default_close_socket;
}
#endif
