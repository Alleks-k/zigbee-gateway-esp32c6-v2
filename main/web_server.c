#include "web_server.h"
#include "esp_log.h"
#include "esp_zigbee_gateway.h"
#include "device_manager.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
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

/* Helper: Створення JSON зі статусом */
static char* create_status_json(void)
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(root, "pan_id", esp_zb_get_pan_id());
    cJSON_AddNumberToObject(root, "channel", esp_zb_get_current_channel());
    cJSON_AddNumberToObject(root, "short_addr", esp_zb_get_short_address());

    /* Отримуємо список пристроїв з менеджера */
    cJSON *dev_list = device_manager_get_json_list();

    cJSON_AddItemToObject(root, "devices", dev_list);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* API: Статус у JSON */
esp_err_t api_status_handler(httpd_req_t *req)
{
    char *json_str = create_status_json();
    if (!json_str) return ESP_FAIL;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void*)json_str);
    return ESP_OK;
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

/* API: Відкрити мережу (Permit Join) */
esp_err_t api_permit_join_handler(httpd_req_t *req)
{
    esp_zb_bdb_open_network(60);
    ESP_LOGI(TAG, "Network opened for 60 seconds via Web API");
    const char* resp = "{\"message\":\"Network opened for 60 seconds\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* API: Керування пристроєм */
esp_err_t api_control_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
    cJSON *ep_item = cJSON_GetObjectItem(root, "ep");
    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");

    if (cJSON_IsNumber(addr_item) && cJSON_IsNumber(ep_item) && cJSON_IsNumber(cmd_item)) {
        uint16_t addr = (uint16_t)addr_item->valueint;
        uint8_t endpoint = (uint8_t)ep_item->valueint;
        uint8_t cmd = (uint8_t)cmd_item->valueint;

        ESP_LOGI(TAG, "Web Control: addr=0x%04x, ep=%d, cmd=%d", addr, endpoint, cmd);
        send_on_off_command(addr, endpoint, cmd);
        const char* resp = "{\"status\":\"ok\", \"message\":\"Command sent\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, resp);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    }
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Видалення пристрою */
esp_err_t api_delete_device_handler(httpd_req_t *req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *addr_item = cJSON_GetObjectItem(root, "short_addr");
        if (cJSON_IsNumber(addr_item)) {
            delete_device((uint16_t)addr_item->valueint);
            const char* resp = "{\"status\":\"ok\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid address");
        }
        cJSON_Delete(root);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }
    return ESP_OK;
}

/* clarawlan7: Збереження налаштувань Wi-Fi */
esp_err_t api_wifi_save_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "password");

    if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
        size_t ssid_len = strlen(ssid->valuestring);
        size_t pass_len = strlen(pass->valuestring);

        if (ssid_len == 0 || ssid_len > 32) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID length (1-32)");
            cJSON_Delete(root);
            return ESP_OK;
        }

        if (pass_len < 8 || pass_len > 64) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Password length (8-64)");
            cJSON_Delete(root);
            return ESP_OK;
        }

        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err == ESP_OK) {
            nvs_set_str(my_handle, "wifi_ssid", ssid->valuestring);
            nvs_set_str(my_handle, "wifi_pass", pass->valuestring);
            nvs_commit(my_handle);
            nvs_close(my_handle);
            
            const char* resp = "{\"status\":\"ok\", \"message\":\"Saved. Restarting...\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
            
            /* Даємо час на відправку відповіді перед перезавантаженням */
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            httpd_resp_send_500(req);
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
    }
    cJSON_Delete(root);
    return ESP_OK;
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

        httpd_uri_t uri_wifi = { .uri = "/api/settings/wifi", .method = HTTP_POST, .handler = api_wifi_save_handler };
        httpd_register_uri_handler(server, &uri_wifi);

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