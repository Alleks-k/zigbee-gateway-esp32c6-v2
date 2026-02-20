#include "web_server.h"
#include "http_routes.h"
#include "ws_manager.h"
#include "mdns_service.h"
#include "esp_log.h"
#include "esp_err.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
#error "WebSocket support is not enabled! Please run 'idf.py menuconfig', go to 'Component config' -> 'HTTP Server' and enable 'WebSocket support'."
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;
static ws_manager_handle_t s_ws_manager = NULL;

static void web_server_close_fn(httpd_handle_t hd, int sockfd)
{
    ws_httpd_close_fn_with_handle(s_ws_manager, hd, sockfd);
}

void start_web_server(api_usecases_handle_t usecases)
{
    server = NULL;
    if (!usecases) {
        ESP_LOGE(TAG, "API usecases handle is required");
        return;
    }
    if (!s_ws_manager) {
        if (ws_manager_create(&s_ws_manager) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create WS manager");
            return;
        }
    }
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = 80;
    // Keep extra margin for telemetry/status JSON and WS handling.
    httpd_config.stack_size += 2048;
    // Static + WS + API(v1 + legacy aliases) + wildcard routes require higher capacity.
    httpd_config.max_uri_handlers = 48;
    // Enable wildcard matching for dynamic routes like /api/v1/jobs/*.
    httpd_config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_config.close_fn = web_server_close_fn;

    ESP_LOGI(TAG, "Starting Web Server on port %d", httpd_config.server_port);
    if (httpd_start(&server, &httpd_config) == ESP_OK) {
        ws_manager_init_with_handle(s_ws_manager, server, usecases);
        if (!http_routes_register(server, s_ws_manager, usecases)) {
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
    if (s_ws_manager) {
        ws_manager_destroy(s_ws_manager);
        s_ws_manager = NULL;
    }
}

int web_server_get_ws_client_count(void)
{
    return ws_manager_get_client_count_with_handle(s_ws_manager);
}

void web_server_broadcast_ws_status(void)
{
    ws_broadcast_status_with_handle(s_ws_manager);
}
