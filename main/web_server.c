#include "web_server.h"
#include "api_handlers.h"
#include "esp_log.h"
#include "device_manager.h"
#include <stdio.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_system.h"
#include "mdns.h"
#include "hostname_settings.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
#error "WebSocket support is not enabled! Please run 'idf.py menuconfig', go to 'Component config' -> 'HTTP Server' and enable 'WebSocket support'."
#endif

static const char *TAG = "WEB_SERVER";

/* WebSocket Globals */
#define MAX_WS_CLIENTS 8
static int ws_fds[MAX_WS_CLIENTS];
static httpd_handle_t server = NULL;
static SemaphoreHandle_t ws_mutex = NULL;

// Функції пристроїв перенесено в device_manager.c

static void ws_remove_fd(int fd)
{
    if (ws_mutex) {
        xSemaphoreTake(ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] == fd) {
            ws_fds[i] = -1;
            break;
        }
    }
    if (ws_mutex) {
        xSemaphoreGive(ws_mutex);
    }
}

static void ws_httpd_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    ws_remove_fd(sockfd);
    close(sockfd);
}

/* Допоміжна функція для віддачі файлів зі SPIFFS */
static esp_err_t serve_spiffs_file(httpd_req_t *req, const char *filepath, const char *content_type)
{
    FILE* f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File stat failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    char buf[1024];
    size_t len = 0;
    while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Обробник для головної сторінки */
esp_err_t web_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/index.html", "text/html; charset=utf-8");
}

/* Обробник для CSS */
esp_err_t css_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/style.css", "text/css");
}

/* Обробник для JS */
esp_err_t js_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/script.js", "application/javascript");
}

/* WebSocket: Обробник з'єднань */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WS connection");
        int fd = httpd_req_to_sockfd(req);
        bool added = false;
        if (ws_mutex) {
            xSemaphoreTake(ws_mutex, portMAX_DELAY);
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
        if (ws_mutex) {
            xSemaphoreGive(ws_mutex);
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
    if (ret != ESP_OK) return ret;

    /* Якщо отримали CLOSE фрейм - звільняємо слот */
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        ws_remove_fd(fd);
    }
    return ESP_OK;
}

static bool register_uri_handler_checked(httpd_handle_t srv, const httpd_uri_t *uri)
{
    esp_err_t ret = httpd_register_uri_handler(srv, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI %s: %s", uri->uri, esp_err_to_name(ret));
        return false;
    }
    return true;
}

/* WebSocket: Розсилка статусу всім клієнтам */
void ws_broadcast_status(void) {
    if (!server) return;

    char *json_str = create_status_json();
    if (!json_str) return;
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)json_str;
    ws_pkt.len = strlen(json_str);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    if (ws_mutex) {
        xSemaphoreTake(ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            // Відправляємо асинхронно, ігноруємо помилки (клієнт міг відпасти)
            esp_err_t ret = httpd_ws_send_frame_async(server, ws_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "WS send failed (%s), removing client %d", esp_err_to_name(ret), ws_fds[i]);
                ws_fds[i] = -1; // Видаляємо неактивного клієнта
            }
        }
    }
    if (ws_mutex) {
        xSemaphoreGive(ws_mutex);
    }
    free(json_str);
}

/* Обробник для favicon */
esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void start_mdns_service(void)
{
    esp_err_t err = mdns_init();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "mDNS already initialized");
    } else if (err) {
        ESP_LOGE(TAG, "mDNS Init failed: %d", err);
        return;
    }
    mdns_hostname_set(GATEWAY_MDNS_HOSTNAME);
    mdns_instance_name_set(GATEWAY_MDNS_INSTANCE);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://%s.local", GATEWAY_MDNS_HOSTNAME);
}

void start_web_server(void)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) ws_fds[i] = -1;

    if (!ws_mutex) {
        ws_mutex = xSemaphoreCreateMutex();
        if (!ws_mutex) {
            ESP_LOGE(TAG, "Failed to create WS mutex");
            return;
        }
    }
    
    device_manager_init();

    server = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;
    httpd_config.max_uri_handlers = 16; // Потрібно щонайменше 13 URI, залишаємо запас
    httpd_config.close_fn = ws_httpd_close_fn;

    ESP_LOGI(TAG, "Starting Web Server on port %d", httpd_config.server_port);
    if (httpd_start(&server, &httpd_config) == ESP_OK) {
        bool ok = true;

        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = web_handler };
        ok &= register_uri_handler_checked(server, &uri_get);

        httpd_uri_t uri_css = { .uri = "/style.css", .method = HTTP_GET, .handler = css_handler };
        ok &= register_uri_handler_checked(server, &uri_css);

        httpd_uri_t uri_js = { .uri = "/script.js", .method = HTTP_GET, .handler = js_handler };
        ok &= register_uri_handler_checked(server, &uri_js);

        httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler };
        ok &= register_uri_handler_checked(server, &uri_status);

        httpd_uri_t uri_permit = { .uri = "/api/permit_join", .method = HTTP_POST, .handler = api_permit_join_handler };
        ok &= register_uri_handler_checked(server, &uri_permit);

        httpd_uri_t uri_control = { .uri = "/api/control", .method = HTTP_POST, .handler = api_control_handler };
        ok &= register_uri_handler_checked(server, &uri_control);

        httpd_uri_t uri_delete = { .uri = "/api/delete", .method = HTTP_POST, .handler = api_delete_device_handler };
        ok &= register_uri_handler_checked(server, &uri_delete);

        httpd_uri_t uri_rename = { .uri = "/api/rename", .method = HTTP_POST, .handler = api_rename_device_handler };
        ok &= register_uri_handler_checked(server, &uri_rename);

        httpd_uri_t uri_scan = { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = api_wifi_scan_handler };
        ok &= register_uri_handler_checked(server, &uri_scan);

        httpd_uri_t uri_wifi = { .uri = "/api/settings/wifi", .method = HTTP_POST, .handler = api_wifi_save_handler };
        ok &= register_uri_handler_checked(server, &uri_wifi);

        httpd_uri_t uri_reboot = { .uri = "/api/reboot", .method = HTTP_POST, .handler = api_reboot_handler };
        ok &= register_uri_handler_checked(server, &uri_reboot);

        httpd_uri_t uri_ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };
        ok &= register_uri_handler_checked(server, &uri_ws);

        httpd_uri_t uri_favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        ok &= register_uri_handler_checked(server, &uri_favicon);

        if (!ok) {
            ESP_LOGE(TAG, "Web server init failed: one or more URI handlers were not registered");
            httpd_stop(server);
            server = NULL;
            return;
        }

        ESP_LOGI(TAG, "Web server handlers registered successfully");
        start_mdns_service();
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}
