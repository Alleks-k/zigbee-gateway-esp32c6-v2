#include "web_server.h"
#include "http_routes.h"
#include "ws_manager.h"
#include "mdns_service.h"
#include "esp_log.h"
#include "esp_err.h"
#include "device_manager.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
#error "WebSocket support is not enabled! Please run 'idf.py menuconfig', go to 'Component config' -> 'HTTP Server' and enable 'WebSocket support'."
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

void start_web_server(void)
{
    esp_err_t dev_mgr_ret = device_manager_init();
    if (dev_mgr_ret != ESP_OK) {
        ESP_LOGE(TAG, "device_manager_init failed: %s", esp_err_to_name(dev_mgr_ret));
        return;
    }

    server = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;
    // Keep extra margin for telemetry/status JSON and WS handling.
    httpd_config.stack_size += 2048;
    // Static + WS + API(v1 + legacy aliases) + wildcard routes require higher capacity.
    httpd_config.max_uri_handlers = 48;
    // Enable wildcard matching for dynamic routes like /api/v1/jobs/*.
    httpd_config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_config.close_fn = ws_httpd_close_fn;

    ESP_LOGI(TAG, "Starting Web Server on port %d", httpd_config.server_port);
    if (httpd_start(&server, &httpd_config) == ESP_OK) {
        ws_manager_init(server);
        if (!http_routes_register(server)) {
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

void stop_web_server(void)
{
    if (server) {
        ESP_LOGI(TAG, "Stopping Web Server");
        httpd_stop(server);
        server = NULL;
    }
}
