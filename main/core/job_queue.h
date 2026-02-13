#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define ZGW_JOB_RESULT_MAX_LEN 2048

typedef enum {
    ZGW_JOB_TYPE_WIFI_SCAN = 0,
    ZGW_JOB_TYPE_FACTORY_RESET,
    ZGW_JOB_TYPE_REBOOT,
    ZGW_JOB_TYPE_UPDATE,
} zgw_job_type_t;

typedef enum {
    ZGW_JOB_STATE_QUEUED = 0,
    ZGW_JOB_STATE_RUNNING,
    ZGW_JOB_STATE_SUCCEEDED,
    ZGW_JOB_STATE_FAILED,
} zgw_job_state_t;

typedef struct {
    uint32_t id;
    zgw_job_type_t type;
    zgw_job_state_t state;
    esp_err_t err;
    uint64_t created_ms;
    uint64_t updated_ms;
    bool has_result;
    char result_json[ZGW_JOB_RESULT_MAX_LEN];
} zgw_job_info_t;

esp_err_t job_queue_init(void);
esp_err_t job_queue_submit(zgw_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id);
esp_err_t job_queue_get(uint32_t job_id, zgw_job_info_t *out_info);

const char *job_queue_type_to_string(zgw_job_type_t type);
const char *job_queue_state_to_string(zgw_job_state_t state);

