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

bool http_routes_register(httpd_handle_t server)
{
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

    httpd_uri_t uri_factory_reset = { .uri = "/api/factory_reset", .method = HTTP_POST, .handler = api_factory_reset_handler };
    ok &= register_uri_handler_checked(server, &uri_factory_reset);

    httpd_uri_t uri_ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };
    ok &= register_uri_handler_checked(server, &uri_ws);

    httpd_uri_t uri_favicon = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler };
    ok &= register_uri_handler_checked(server, &uri_favicon);

    return ok;
}
