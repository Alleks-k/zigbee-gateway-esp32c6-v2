#include "http_error.h"
#include "error_ring.h"
#include <stdio.h>
#include <string.h>

__attribute__((weak)) bool http_error_map_provider_hook(esp_err_t err, int *out_http_status, const char **out_error_code)
{
    (void)err;
    (void)out_http_status;
    (void)out_error_code;
    return false;
}

static const char *http_status_text(int code)
{
    switch (code) {
    case 400: return "400 Bad Request";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 500: return "500 Internal Server Error";
    case 503: return "503 Service Unavailable";
    default: return "500 Internal Server Error";
    }
}

static bool map_error_default(esp_err_t err, int *out_http_status, const char **out_error_code)
{
    if (!out_http_status || !out_error_code) {
        return false;
    }

    switch (err) {
    case ESP_ERR_INVALID_ARG:
        *out_http_status = 400;
        *out_error_code = "invalid_argument";
        return true;
    case ESP_ERR_NOT_FOUND:
        *out_http_status = 404;
        *out_error_code = "not_found";
        return true;
    case ESP_ERR_INVALID_STATE:
        *out_http_status = 409;
        *out_error_code = "invalid_state";
        return true;
    case ESP_ERR_NO_MEM:
        *out_http_status = 503;
        *out_error_code = "no_memory";
        return true;
    default:
        *out_http_status = 500;
        *out_error_code = "internal_error";
        return true;
    }
}

static bool map_error(esp_err_t err, int *out_http_status, const char **out_error_code)
{
    if (http_error_map_provider_hook(err, out_http_status, out_error_code)) {
        return true;
    }
    return map_error_default(err, out_http_status, out_error_code);
}

esp_err_t http_error_send(httpd_req_t *req, int http_status, const char *code, const char *message)
{
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *err_code = code ? code : "internal_error";
    const char *err_msg = message ? message : "Internal error";

    char body[192];
    int written = snprintf(
        body, sizeof(body),
        "{\"status\":\"error\",\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
        err_code, err_msg
    );
    if (written < 0 || (size_t)written >= sizeof(body)) {
        strncpy(body, "{\"status\":\"error\",\"error\":{\"code\":\"internal_error\",\"message\":\"Internal error\"}}", sizeof(body));
        body[sizeof(body) - 1] = '\0';
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, http_status_text(http_status));
    return httpd_resp_sendstr(req, body);
}

esp_err_t http_error_send_esp(httpd_req_t *req, esp_err_t err, const char *message)
{
    int http_status = 500;
    const char *error_code = "internal_error";
    (void)map_error(err, &http_status, &error_code);

    gateway_error_ring_add("api", (int32_t)err, message ? message : "api error");
    return http_error_send(req, http_status, error_code, message);
}

esp_err_t http_success_send(httpd_req_t *req, const char *message)
{
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *msg = message ? message : "ok";
    char data_obj[160];
    int written = snprintf(data_obj, sizeof(data_obj), "{\"message\":\"%s\"}", msg);
    if (written < 0 || (size_t)written >= sizeof(data_obj)) {
        return http_success_send_data_json(req, "{\"message\":\"ok\"}");
    }
    return http_success_send_data_json(req, data_obj);
}

esp_err_t http_success_send_data_json(httpd_req_t *req, const char *data_json)
{
    if (!req || !data_json) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr_chunk(req, "{\"status\":\"ok\",\"data\":");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = httpd_resp_sendstr_chunk(req, data_json);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = httpd_resp_sendstr_chunk(req, "}");
    if (ret != ESP_OK) {
        return ret;
    }
    return httpd_resp_sendstr_chunk(req, NULL);
}
