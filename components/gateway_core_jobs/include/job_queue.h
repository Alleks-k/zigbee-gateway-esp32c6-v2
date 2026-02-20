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
    ZGW_JOB_TYPE_LQI_REFRESH,
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

typedef struct {
    uint32_t submitted_total;
    uint32_t dedup_reused_total;
    uint32_t completed_total;
    uint32_t failed_total;
    uint32_t queue_depth_current;
    uint32_t queue_depth_peak;
    uint32_t latency_p95_ms;
} zgw_job_metrics_t;

typedef struct zgw_job_queue *job_queue_handle_t;
typedef struct zigbee_service *zigbee_service_handle_t;
struct wifi_service;
struct system_service;

esp_err_t job_queue_create(job_queue_handle_t *out_handle);
void job_queue_destroy(job_queue_handle_t handle);
esp_err_t job_queue_init_with_handle(job_queue_handle_t handle);
esp_err_t job_queue_set_zigbee_service_with_handle(job_queue_handle_t handle, zigbee_service_handle_t zigbee_service_handle);
esp_err_t job_queue_set_platform_services_with_handle(job_queue_handle_t handle,
                                                      struct wifi_service *wifi_service_handle,
                                                      struct system_service *system_service_handle);
esp_err_t job_queue_submit_with_handle(job_queue_handle_t handle, zgw_job_type_t type, uint32_t reboot_delay_ms,
                                       uint32_t *out_job_id);
esp_err_t job_queue_get_with_handle(job_queue_handle_t handle, uint32_t job_id, zgw_job_info_t *out_info);
esp_err_t job_queue_get_metrics_with_handle(job_queue_handle_t handle, zgw_job_metrics_t *out_metrics);

const char *job_queue_type_to_string(zgw_job_type_t type);
const char *job_queue_state_to_string(zgw_job_state_t state);
