#include "api_handlers.h"
#include "status_json_builder.h"
#include "http_error.h"
#include "lqi_json_mapper.h"

#include <stdlib.h>

esp_err_t api_status_handler(httpd_req_t *req)
{
    char *json_str = create_status_json();
    if (!json_str) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build status payload");
    }
    esp_err_t ret = http_success_send_data_json(req, json_str);
    free((void *)json_str);
    if (ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap status payload");
    }
    return ret;
}

esp_err_t api_lqi_handler(httpd_req_t *req)
{
    size_t needed = 1024;
    for (int i = 0; i < 4; i++) {
        char *json = (char *)malloc(needed);
        if (!json) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Out of memory");
        }

        size_t out_len = 0;
        esp_err_t ret = build_lqi_json_compact(json, needed, &out_len);
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

