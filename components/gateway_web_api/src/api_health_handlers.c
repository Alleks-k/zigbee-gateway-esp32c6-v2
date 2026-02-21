#include "api_handlers.h"
#include "health_json_builder.h"
#include "http_error.h"

#include <stdlib.h>

#define HEALTH_JSON_STACK_CAP 768
#define HEALTH_JSON_HEAP_RETRIES 3

static api_usecases_handle_t req_usecases(httpd_req_t *req)
{
    return req ? (api_usecases_handle_t)req->user_ctx : NULL;
}

esp_err_t api_health_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    size_t health_len = 0;
    char stack_json[HEALTH_JSON_STACK_CAP];
    esp_err_t ret = build_health_json_compact(usecases, stack_json, sizeof(stack_json), &health_len);
    if (ret == ESP_OK) {
        esp_err_t send_ret = http_success_send_data_json(req, stack_json);
        if (send_ret == ESP_ERR_NO_MEM) {
            return http_error_send(req, 503, "no_memory", "Failed to wrap health payload");
        }
        return send_ret;
    }
    if (ret != ESP_ERR_NO_MEM) {
        return http_error_send(req, 500, "internal_error", "Failed to build health payload");
    }

    size_t needed = HEALTH_JSON_STACK_CAP * 2;
    for (int i = 0; i < HEALTH_JSON_HEAP_RETRIES; i++) {
        char *health_json = (char *)malloc(needed);
        if (!health_json) {
            return http_error_send(req, 503, "no_memory", "Out of memory");
        }

        ret = build_health_json_compact(usecases, health_json, needed, &health_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, health_json);
            free(health_json);
            if (send_ret == ESP_ERR_NO_MEM) {
                return http_error_send(req, 503, "no_memory", "Failed to wrap health payload");
            }
            return send_ret;
        }

        free(health_json);
        if (ret != ESP_ERR_NO_MEM) {
            return http_error_send(req, 500, "internal_error", "Failed to build health payload");
        }
        needed *= 2;
    }

    return http_error_send(req, 503, "no_memory", "Health payload too large");
}
