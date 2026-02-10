#include "web_server.h"
#include "esp_log.h"
#include "esp_zigbee_gateway.h"
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

/* Глобальні дані пристроїв */
static SemaphoreHandle_t devices_mutex = NULL;
zb_device_t devices[MAX_DEVICES];
int device_count = 0;

void ws_broadcast_status(void);

/* Функція для збереження списку пристроїв у постійну пам'ять (NVS) */
void save_devices_to_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_i32(my_handle, "dev_count", device_count);
        nvs_set_blob(my_handle, "dev_list", devices, sizeof(zb_device_t) * MAX_DEVICES);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device list successfully saved to NVS");
        } else {
            ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Error opening NVS for writing: %s", esp_err_to_name(err));
    }
}

/* Функція для завантаження списку пристроїв при старті */
void load_devices_from_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(my_handle, "dev_count", &count);
        device_count = (int)count;
        
        size_t size = sizeof(zb_device_t) * MAX_DEVICES;
        nvs_get_blob(my_handle, "dev_list", devices, &size);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Loaded %d devices from NVS", device_count);
    } else {
        ESP_LOGW(TAG, "No device data found in NVS (First boot?)");
        device_count = 0;
    }
}

/* Функція додавання пристрою з IEEE адресою */
void add_device_with_ieee(uint16_t addr, esp_zb_ieee_addr_t ieee) {
    if (devices_mutex != NULL) {
        xSemaphoreTake(devices_mutex, portMAX_DELAY);
    }

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            ESP_LOGI(TAG, "Device 0x%04x is already in the list, updating IEEE", addr);
            memcpy(devices[i].ieee_addr, ieee, sizeof(esp_zb_ieee_addr_t));
            if (devices_mutex != NULL) {
                xSemaphoreGive(devices_mutex);
            }
            return;
        }
    }

    if (device_count < MAX_DEVICES) {
        devices[device_count].short_addr = addr;
        memcpy(devices[device_count].ieee_addr, ieee, sizeof(esp_zb_ieee_addr_t));
        snprintf(devices[device_count].name, sizeof(devices[device_count].name), "Пристрій 0x%04x", addr);
        device_count++;
        
        ESP_LOGI(TAG, "New device added: 0x%04x. Total: %d", addr, device_count);
        save_devices_to_nvs();
    } else {
        ESP_LOGW(TAG, "Maximum device limit reached (%d)", MAX_DEVICES);
    }

    if (devices_mutex != NULL) {
        xSemaphoreGive(devices_mutex);
    }
    ws_broadcast_status();
}

/* Функція видалення пристрою */
void delete_device(uint16_t addr) {
    if (devices_mutex != NULL) {
        xSemaphoreTake(devices_mutex, portMAX_DELAY);
    }

    int found_idx = -1;
    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == addr) {
            found_idx = i;
            break;
        }
    }

    if (found_idx != -1) {
        // 1. Закриваємо мережу, щоб пристрій не підключився автоматично назад
        esp_zb_bdb_open_network(0);
        ESP_LOGW(TAG, "Network closed to prevent immediate re-pairing");

        // 2. Надсилаємо команду Leave пристрою
        send_leave_command(devices[found_idx].short_addr, devices[found_idx].ieee_addr);

        // 3. Видаляємо з локального списку
        for (int i = found_idx; i < device_count - 1; i++) {
            devices[i] = devices[i + 1];
        }
        device_count--;
        ESP_LOGI(TAG, "Device 0x%04x removed. Remaining: %d", addr, device_count);
        save_devices_to_nvs();
    }

    if (devices_mutex != NULL) {
        xSemaphoreGive(devices_mutex);
    }
    ws_broadcast_status();
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

    if (devices_mutex != NULL) {
        xSemaphoreTake(devices_mutex, portMAX_DELAY);
    }

    cJSON *dev_list = cJSON_CreateArray();
    for (int i = 0; i < device_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", devices[i].name);
        cJSON_AddNumberToObject(item, "short_addr", devices[i].short_addr);
        cJSON_AddItemToArray(dev_list, item);
    }

    if (devices_mutex != NULL) {
        xSemaphoreGive(devices_mutex);
    }

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
    char buf[128];
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
    devices_mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_WS_CLIENTS; i++) ws_fds[i] = -1;
    
    load_devices_from_nvs();

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