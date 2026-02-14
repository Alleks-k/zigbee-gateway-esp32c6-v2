#include "http_routes.h"
#include "api_handlers.h"
#include "http_static.h"
#include "ws_manager.h"
#include "esp_log.h"

static const char *TAG = "HTTP_ROUTES";

static bool register_uri_handler_checked(httpd_handle_t srv, const httpd_uri_t *uri)
{
    esp_err_t ret = httpd_register_uri_handler(srv, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI %s: %s", uri->uri, esp_err_to_name(ret));
        return false;
    }
    return true;
}

#define REGISTER_API_ROUTE_BOTH(_server, _suffix, _method, _handler, _ok)                                       \
    do {                                                                                                          \
        httpd_uri_t uri_v1 = { .uri = "/api/v1" _suffix, .method = _method, .handler = _handler };              \
        (_ok) &= register_uri_handler_checked((_server), &uri_v1);                                                \
        httpd_uri_t uri_legacy = { .uri = "/api" _suffix, .method = _method, .handler = _handler };             \
        (_ok) &= register_uri_handler_checked((_server), &uri_legacy);                                            \
    } while (0)

bool http_routes_register(httpd_handle_t server)
{
    bool ok = true;

    httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = web_handler };
    ok &= register_uri_handler_checked(server, &uri_get);

    httpd_uri_t uri_css = { .uri = "/style.css", .method = HTTP_GET, .handler = css_handler };
    ok &= register_uri_handler_checked(server, &uri_css);

    httpd_uri_t uri_js = { .uri = "/script.js", .method = HTTP_GET, .handler = js_handler };
    ok &= register_uri_handler_checked(server, &uri_js);

    REGISTER_API_ROUTE_BOTH(server, "/status", HTTP_GET, api_status_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/lqi", HTTP_GET, api_lqi_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/health", HTTP_GET, api_health_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/permit_join", HTTP_POST, api_permit_join_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/control", HTTP_POST, api_control_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/delete", HTTP_POST, api_delete_device_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/rename", HTTP_POST, api_rename_device_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/wifi/scan", HTTP_GET, api_wifi_scan_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/settings/wifi", HTTP_POST, api_wifi_save_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/reboot", HTTP_POST, api_reboot_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/factory_reset", HTTP_POST, api_factory_reset_handler, ok);
    REGISTER_API_ROUTE_BOTH(server, "/jobs", HTTP_POST, api_jobs_submit_handler, ok);

    httpd_uri_t uri_jobs_get_v1 = { .uri = "/api/v1/jobs/*", .method = HTTP_GET, .handler = api_jobs_get_handler };
    ok &= register_uri_handler_checked(server, &uri_jobs_get_v1);
    httpd_uri_t uri_jobs_get_legacy = { .uri = "/api/jobs/*", .method = HTTP_GET, .handler = api_jobs_get_handler };
    ok &= register_uri_handler_checked(server, &uri_jobs_get_legacy);

    httpd_uri_t uri_ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };
    ok &= register_uri_handler_checked(server, &uri_ws);

    httpd_uri_t uri_favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
    ok &= register_uri_handler_checked(server, &uri_favicon);

    return ok;
}
