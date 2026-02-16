#include "api_handlers.h"
#include "health_json_builder.h"
#include "http_error.h"

#include <stdlib.h>

esp_err_t api_health_handler(httpd_req_t *req)
{
    size_t needed = 768;
    for (int i = 0; i < 4; i++) {
        char *health_json = (char *)malloc(needed);
        if (!health_json) {
            return http_error_send(req, 503, "no_memory", "Out of memory");
        }

        size_t health_len = 0;
        esp_err_t ret = build_health_json_compact(health_json, needed, &health_len);
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

