#include "api_handlers.h"
#include "status_json_builder.h"
#include "http_error.h"
#include "lqi_json_mapper.h"

#include <stdlib.h>

#define STATUS_JSON_STACK_CAP 1024
#define STATUS_JSON_HEAP_RETRIES 3
#define LQI_JSON_STACK_CAP 1024
#define LQI_JSON_HEAP_RETRIES 3

static api_usecases_handle_t req_usecases(httpd_req_t *req)
{
    return req ? (api_usecases_handle_t)req->user_ctx : NULL;
}

esp_err_t api_status_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    char stack_json[STATUS_JSON_STACK_CAP];
    size_t out_len = 0;
    esp_err_t ret = build_status_json_compact(usecases, stack_json, sizeof(stack_json), &out_len);
    if (ret == ESP_OK) {
        esp_err_t send_ret = http_success_send_data_json(req, stack_json);
        if (send_ret == ESP_ERR_NO_MEM) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap status payload");
        }
        return send_ret;
    }
    if (ret != ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ret, "Failed to build status payload");
    }

    size_t needed = STATUS_JSON_STACK_CAP * 2;
    for (int i = 0; i < STATUS_JSON_HEAP_RETRIES; i++) {
        char *heap_json = (char *)malloc(needed);
        if (!heap_json) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build status payload");
        }
        ret = build_status_json_compact(usecases, heap_json, needed, &out_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, heap_json);
            free(heap_json);
            if (send_ret == ESP_ERR_NO_MEM) {
                return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap status payload");
            }
            return send_ret;
        }
        free(heap_json);
        if (ret != ESP_ERR_NO_MEM) {
            return http_error_send_esp(req, ret, "Failed to build status payload");
        }
        needed *= 2;
    }

    return http_error_send_esp(req, ESP_ERR_NO_MEM, "Status payload too large");
}

esp_err_t api_lqi_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    size_t out_len = 0;

    char stack_json[LQI_JSON_STACK_CAP];
    esp_err_t ret = build_lqi_json_compact(usecases, stack_json, sizeof(stack_json), &out_len);
    if (ret == ESP_OK) {
        esp_err_t send_ret = http_success_send_data_json(req, stack_json);
        if (send_ret == ESP_ERR_NO_MEM) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "LQI payload too large");
        }
        return send_ret;
    }
    if (ret != ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ret, "Failed to build LQI payload");
    }

    size_t needed = LQI_JSON_STACK_CAP * 2;
    for (int i = 0; i < LQI_JSON_HEAP_RETRIES; i++) {
        char *json = (char *)malloc(needed);
        if (!json) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Out of memory");
        }
        ret = build_lqi_json_compact(usecases, json, needed, &out_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, json);
            free(json);
            if (send_ret == ESP_ERR_NO_MEM) {
                return http_error_send_esp(req, ESP_ERR_NO_MEM, "LQI payload too large");
            }
            return send_ret;
        }
        free(json);
        if (ret != ESP_ERR_NO_MEM) {
            return http_error_send_esp(req, ret, "Failed to build LQI payload");
        }
        needed *= 2;
    }

    return http_error_send_esp(req, ESP_ERR_NO_MEM, "LQI payload too large");
}
