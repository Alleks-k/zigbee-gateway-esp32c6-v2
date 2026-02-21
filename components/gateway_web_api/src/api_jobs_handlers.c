#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "gateway_jobs_facade.h"
#include "http_error.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Guardrails for /api*/jobs/{id}: bound result payload by job type.
#define JOB_API_RESULT_JSON_LIMIT_SCAN          768
#define JOB_API_RESULT_JSON_LIMIT_FACTORY_RESET 1536
#define JOB_API_RESULT_JSON_LIMIT_REBOOT        512
#define JOB_API_RESULT_JSON_LIMIT_UPDATE        768
#define JOB_API_RESULT_JSON_LIMIT_LQI_REFRESH   1024
static size_t job_result_json_limit_for_type(gateway_core_job_type_t type)
{
    switch (type) {
    case GATEWAY_CORE_JOB_TYPE_WIFI_SCAN:
        return JOB_API_RESULT_JSON_LIMIT_SCAN;
    case GATEWAY_CORE_JOB_TYPE_FACTORY_RESET:
        return JOB_API_RESULT_JSON_LIMIT_FACTORY_RESET;
    case GATEWAY_CORE_JOB_TYPE_REBOOT:
        return JOB_API_RESULT_JSON_LIMIT_REBOOT;
    case GATEWAY_CORE_JOB_TYPE_LQI_REFRESH:
        return JOB_API_RESULT_JSON_LIMIT_LQI_REFRESH;
    case GATEWAY_CORE_JOB_TYPE_UPDATE:
    default:
        return JOB_API_RESULT_JSON_LIMIT_UPDATE;
    }
}

static gateway_core_job_type_t parse_job_type(const char *type)
{
    if (!type) {
        return GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
    }
    if (strcmp(type, "scan") == 0) {
        return GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
    }
    if (strcmp(type, "factory_reset") == 0) {
        return GATEWAY_CORE_JOB_TYPE_FACTORY_RESET;
    }
    if (strcmp(type, "reboot") == 0) {
        return GATEWAY_CORE_JOB_TYPE_REBOOT;
    }
    if (strcmp(type, "update") == 0) {
        return GATEWAY_CORE_JOB_TYPE_UPDATE;
    }
    if (strcmp(type, "lqi_refresh") == 0) {
        return GATEWAY_CORE_JOB_TYPE_LQI_REFRESH;
    }
    return GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
}

static esp_err_t parse_job_id_from_uri(const char *uri, uint32_t *out_id)
{
    if (!uri || !out_id) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *last_slash = strrchr(uri, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    char *endptr = NULL;
    unsigned long value = strtoul(last_slash + 1, &endptr, 10);
    if (endptr == NULL || *endptr != '\0' || value == 0 || value > UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_id = (uint32_t)value;
    return ESP_OK;
}

static api_usecases_handle_t req_usecases(httpd_req_t *req)
{
    return req ? (api_usecases_handle_t)req->user_ctx : NULL;
}

esp_err_t api_jobs_submit_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    api_job_submit_request_t in = {0};
    esp_err_t err = api_parse_job_submit_request(req, &in);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Invalid job payload");
    }

    gateway_core_job_type_t type = parse_job_type(in.type);
    uint32_t job_id = 0;
    err = api_usecase_jobs_submit(usecases, type, in.reboot_delay_ms, &job_id);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Failed to queue job");
    }

    gateway_core_job_info_t info = {0};
    const char *state = "queued";
    err = api_usecase_jobs_get(usecases, job_id, &info);
    if (err == ESP_OK) {
        state = gateway_jobs_state_to_string(info.state);
    }

    char data_json[160];
    int written = snprintf(data_json, sizeof(data_json),
                           "{\"job_id\":%" PRIu32 ",\"type\":\"%s\",\"state\":\"%s\"}",
                           job_id, gateway_jobs_type_to_string(type), state);
    if (written < 0 || (size_t)written >= sizeof(data_json)) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build job response");
    }
    return http_success_send_data_json(req, data_json);
}

esp_err_t api_jobs_get_handler(httpd_req_t *req)
{
    api_usecases_handle_t usecases = req_usecases(req);
    uint32_t job_id = 0;
    esp_err_t err = parse_job_id_from_uri(req->uri, &job_id);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Invalid job id");
    }

    gateway_core_job_info_t *info = (gateway_core_job_info_t *)calloc(1, sizeof(gateway_core_job_info_t));
    if (!info) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Out of memory");
    }

    err = api_usecase_jobs_get(usecases, job_id, info);
    if (err != ESP_OK) {
        free(info);
        return http_error_send_esp(req, err, "Job not found");
    }

    const char *result_json = "null";
    char truncated_result_json[96];
    size_t result_limit = job_result_json_limit_for_type(info->type);
    if (info->has_result) {
        size_t result_len = strnlen(info->result_json, sizeof(info->result_json));
        if (result_len > result_limit) {
            int t_written = snprintf(
                truncated_result_json, sizeof(truncated_result_json),
                "{\"truncated\":true,\"original_len\":%u,\"max_len\":%u}",
                (unsigned)result_len, (unsigned)result_limit);
            if (t_written < 0 || (size_t)t_written >= sizeof(truncated_result_json)) {
                free(info);
                return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build truncated result");
            }
            result_json = truncated_result_json;
        } else {
            result_json = info->result_json;
        }
    }

    char head_json[256];
    int written = snprintf(
        head_json, sizeof(head_json),
        "{\"job_id\":%" PRIu32 ",\"type\":\"%s\",\"state\":\"%s\",\"done\":%s,"
        "\"created_ms\":%" PRIu64 ",\"updated_ms\":%" PRIu64 ",\"error\":\"%s\",\"result\":",
        info->id,
        gateway_jobs_type_to_string(info->type),
        gateway_jobs_state_to_string(info->state),
        (info->state == GATEWAY_CORE_JOB_STATE_SUCCEEDED || info->state == GATEWAY_CORE_JOB_STATE_FAILED) ? "true" : "false",
        info->created_ms,
        info->updated_ms,
        esp_err_to_name(info->err));
    if (written < 0 || (size_t)written >= sizeof(head_json)) {
        free(info);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Job response too large");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t send_ret = httpd_resp_sendstr_chunk(req, "{\"status\":\"ok\",\"data\":");
    if (send_ret != ESP_OK) {
        free(info);
        return send_ret;
    }
    send_ret = httpd_resp_sendstr_chunk(req, head_json);
    if (send_ret != ESP_OK) {
        free(info);
        return send_ret;
    }
    send_ret = httpd_resp_sendstr_chunk(req, result_json);
    if (send_ret != ESP_OK) {
        free(info);
        return send_ret;
    }
    send_ret = httpd_resp_sendstr_chunk(req, "}}");
    if (send_ret != ESP_OK) {
        free(info);
        return send_ret;
    }
    send_ret = httpd_resp_sendstr_chunk(req, NULL);
    free(info);
    return send_ret;
}
