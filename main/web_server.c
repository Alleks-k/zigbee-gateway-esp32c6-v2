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

static const char *TAG = "WEB_SERVER";

/* Глобальні дані пристроїв */
static SemaphoreHandle_t devices_mutex = NULL;
zb_device_t devices[MAX_DEVICES];
int device_count = 0;

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
}

/* Обробник для головної сторінки */
esp_err_t web_handler(httpd_req_t *req)
{
    FILE* f = fopen("/www/index.html", "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open index.html");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat("/www/index.html", &st) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char *buf = malloc(st.st_size);
    size_t len = fread(buf, 1, st.st_size, f);
    fclose(f);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

/* Обробник для CSS */
esp_err_t css_handler(httpd_req_t *req)
{
    FILE* f = fopen("/www/style.css", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSS file not found");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat("/www/style.css", &st) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char *buf = malloc(st.st_size);
    size_t len = fread(buf, 1, st.st_size, f);
    fclose(f);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

/* Обробник для JS */
esp_err_t js_handler(httpd_req_t *req)
{
    FILE* f = fopen("/www/script.js", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "JS file not found");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat("/www/script.js", &st) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char *buf = malloc(st.st_size);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t len = fread(buf, 1, st.st_size, f);
    fclose(f);
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, buf, len);
    free(buf);
    return ESP_OK;
}

/* API: Статус у JSON */
esp_err_t api_status_handler(httpd_req_t *req)
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

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void*)json_str);
    cJSON_Delete(root);
    return ESP_OK;
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
    char buf[100];
    int len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    uint16_t addr;
    uint8_t endpoint, cmd;
    
    if (sscanf(buf, "%hu,%hhu,%hhu", &addr, &endpoint, &cmd) == 3) {
        ESP_LOGI(TAG, "Web Control: addr=0x%04x, ep=%d, cmd=%d", addr, endpoint, cmd);
        send_on_off_command(addr, endpoint, cmd);
        const char* resp = "{\"message\":\"Command sent\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid command format");
    }
    return ESP_OK;
}

/* API: Видалення пристрою */
esp_err_t api_delete_device_handler(httpd_req_t *req) {
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    uint16_t addr = (uint16_t)strtol(buf, NULL, 16);
    delete_device(addr);
    
    const char* resp = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

/* Обробник для favicon */
esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void start_web_server(void)
{
    devices_mutex = xSemaphoreCreateMutex();
    load_devices_from_nvs();

    httpd_handle_t server = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;

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

        httpd_uri_t uri_favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
        httpd_register_uri_handler(server, &uri_favicon);

        ESP_LOGI(TAG, "Web server handlers registered successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}