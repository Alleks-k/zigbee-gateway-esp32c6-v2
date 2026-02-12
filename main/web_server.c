#include "web_server.h"
#include "api_handlers.h"
#include "http_static.h"
#include "ws_manager.h"
#include "esp_log.h"
#include "device_manager.h"
#include "mdns.h"
#include "hostname_settings.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
#error "WebSocket support is not enabled! Please run 'idf.py menuconfig', go to 'Component config' -> 'HTTP Server' and enable 'WebSocket support'."
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;


static bool register_uri_handler_checked(httpd_handle_t srv, const httpd_uri_t *uri)
{
    esp_err_t ret = httpd_register_uri_handler(srv, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI %s: %s", uri->uri, esp_err_to_name(ret));
        return false;
    }
    return true;
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
    device_manager_init();

    server = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;
    httpd_config.max_uri_handlers = 16; // Потрібно щонайменше 13 URI, залишаємо запас
    httpd_config.close_fn = ws_httpd_close_fn;

    ESP_LOGI(TAG, "Starting Web Server on port %d", httpd_config.server_port);
    if (httpd_start(&server, &httpd_config) == ESP_OK) {
        ws_manager_init(server);
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
