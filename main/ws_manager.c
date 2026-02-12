#include "ws_manager.h"
#include "api_handlers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "WS_MANAGER";

#define MAX_WS_CLIENTS 8

static int ws_fds[MAX_WS_CLIENTS];
static httpd_handle_t s_server = NULL;
static SemaphoreHandle_t s_ws_mutex = NULL;

static void ws_remove_fd(int fd)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] == fd) {
            ws_fds[i] = -1;
            break;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
}

void ws_manager_init(httpd_handle_t server)
{
    s_server = server;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        ws_fds[i] = -1;
    }

    if (!s_ws_mutex) {
        s_ws_mutex = xSemaphoreCreateMutex();
        if (!s_ws_mutex) {
            ESP_LOGE(TAG, "Failed to create WS mutex");
        }
    }
}

void ws_httpd_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    ws_remove_fd(sockfd);
    close(sockfd);
}

esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WS connection");
        int fd = httpd_req_to_sockfd(req);
        bool added = false;
        if (s_ws_mutex) {
            xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        }
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_fds[i] == fd) {
                added = true;
                break;
            }
        }
        for (int i = 0; i < MAX_WS_CLIENTS && !added; i++) {
            if (ws_fds[i] == -1) {
                ws_fds[i] = fd;
                added = true;
                break;
            }
        }
        if (s_ws_mutex) {
            xSemaphoreGive(s_ws_mutex);
        }
        if (!added) {
            ESP_LOGW(TAG, "WS client rejected: max clients reached (%d)", MAX_WS_CLIENTS);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_send(req, "WS clients limit reached", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        ws_remove_fd(fd);
    }
    return ESP_OK;
}

void ws_broadcast_status(void)
{
    if (!s_server) {
        return;
    }

    char *json_str = create_status_json();
    if (!json_str) {
        return;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)json_str;
    ws_pkt.len = strlen(json_str);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            esp_err_t ret = httpd_ws_send_frame_async(s_server, ws_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "WS send failed (%s), removing client %d", esp_err_to_name(ret), ws_fds[i]);
                ws_fds[i] = -1;
            }
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }

    free(json_str);
}
