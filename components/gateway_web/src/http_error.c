#include "http_error.h"
#include "error_ring.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

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

static int map_http_status(esp_err_t err)
{
    switch (err) {
    case ESP_ERR_INVALID_ARG:
        return 400;
    case ESP_ERR_NOT_FOUND:
    case ESP_ERR_NVS_NOT_FOUND:
        return 404;
    case ESP_ERR_INVALID_STATE:
        return 409;
    case ESP_ERR_NO_MEM:
        return 503;
    default:
        return 500;
    }
}

static const char *map_error_code(esp_err_t err)
{
    switch (err) {
    case ESP_ERR_INVALID_ARG:
        return "invalid_argument";
    case ESP_ERR_NOT_FOUND:
    case ESP_ERR_NVS_NOT_FOUND:
        return "not_found";
    case ESP_ERR_INVALID_STATE:
        return "invalid_state";
    case ESP_ERR_NO_MEM:
        return "no_memory";
    default:
        return "internal_error";
    }
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
    gateway_error_ring_add("api", (int32_t)err, message ? message : "api error");
    return http_error_send(req, map_http_status(err), map_error_code(err), message);
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
