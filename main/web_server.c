#include "web_server.h"
#include "api_handlers.h"
#include "esp_log.h"
#include "device_manager.h"
#include <stdio.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <sys/stat.h>
#include "esp_system.h"
#include "mdns.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
#error "WebSocket support is not enabled! Please run 'idf.py menuconfig', go to 'Component config' -> 'HTTP Server' and enable 'WebSocket support'."
#endif

static const char *TAG = "WEB_SERVER";

/* WebSocket Globals */
#define MAX_WS_CLIENTS 4
static int ws_fds[MAX_WS_CLIENTS];
static httpd_handle_t server = NULL;

// Функції пристроїв перенесено в device_manager.c

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

    char *buf = malloc(st.st_size);
    if (!buf) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate memory for %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    size_t len = fread(buf, 1, st.st_size, f);
    fclose(f);

    httpd_resp_set_type(req, content_type);
    httpd_resp_send(req, buf, len);
    free(buf);
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
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_fds[i] == -1) {
                ws_fds[i] = fd;
                break;
            }
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len) {
        /* Якщо отримали CLOSE фрейм - звільняємо слот */
        if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
            int fd = httpd_req_to_sockfd(req);
            for (int i = 0; i < MAX_WS_CLIENTS; i++) {
                if (ws_fds[i] == fd) {
                    ws_fds[i] = -1;
                    break;
                }
            }
        }
    }
    return ESP_OK;
}

/* WebSocket: Розсилка статусу всім клієнтам */
void ws_broadcast_status(void) {
    char *json_str = create_status_json();
    if (!json_str) return;
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)json_str;
    ws_pkt.len = strlen(json_str);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

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
    if (err) {
        ESP_LOGE(TAG, "mDNS Init failed: %d", err);
        return;
    }
    mdns_hostname_set("zigbee-gw");
    mdns_instance_name_set("ESP32C6 Zigbee Gateway");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://zigbee-gw.local");
}

void start_web_server(void)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) ws_fds[i] = -1;
    
    device_manager_init();

    server = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;
    httpd_config.max_uri_handlers = 12; // Збільшуємо ліміт обробників (за замовчуванням 8)

    ESP_LOGI(TAG, "Starting Web Server on port %d", httpd_config.server_port);
    if (httpd_start(&server, &httpd_config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = web_handler };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_css = { .uri = "/style.css", .method = HTTP_GET, .handler = css_handler };
        httpd_register_uri_handler(server, &uri_css);

        httpd_uri_t uri_js = { .uri = "/script.js", .method = HTTP_GET, .handler = js_handler };
        httpd_register_uri_handler(server, &uri_js);

        httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler };
        httpd_register_uri_handler(server, &uri_status);

        httpd_uri_t uri_permit = { .uri = "/api/permit_join", .method = HTTP_POST, .handler = api_permit_join_handler };
        httpd_register_uri_handler(server, &uri_permit);

        httpd_uri_t uri_control = { .uri = "/api/control", .method = HTTP_POST, .handler = api_control_handler };
        httpd_register_uri_handler(server, &uri_control);

        httpd_uri_t uri_delete = { .uri = "/api/delete", .method = HTTP_POST, .handler = api_delete_device_handler };
        httpd_register_uri_handler(server, &uri_delete);

        httpd_uri_t uri_rename = { .uri = "/api/rename", .method = HTTP_POST, .handler = api_rename_device_handler };
        httpd_register_uri_handler(server, &uri_rename);

        httpd_uri_t uri_scan = { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = api_wifi_scan_handler };
        httpd_register_uri_handler(server, &uri_scan);

        httpd_uri_t uri_wifi = { .uri = "/api/settings/wifi", .method = HTTP_POST, .handler = api_wifi_save_handler };
        httpd_register_uri_handler(server, &uri_wifi);

        httpd_uri_t uri_reboot = { .uri = "/api/reboot", .method = HTTP_POST, .handler = api_reboot_handler };
        httpd_register_uri_handler(server, &uri_reboot);

        httpd_uri_t uri_ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };
        httpd_register_uri_handler(server, &uri_ws);

        httpd_uri_t uri_favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        httpd_register_uri_handler(server, &uri_favicon);

        ESP_LOGI(TAG, "Web server handlers registered successfully");
        start_mdns_service();
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}