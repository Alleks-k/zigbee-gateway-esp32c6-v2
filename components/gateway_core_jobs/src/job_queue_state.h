#pragma once

#include "job_queue.h"

#include <stddef.h>
#include <stdint.h>

#define ZGW_JOB_MAX 12
#define ZGW_JOB_COMPLETED_TTL_MS (30 * 1000ULL)

typedef struct {
    bool used;
    uint32_t id;
    zgw_job_type_t type;
    zgw_job_state_t state;
    esp_err_t err;
    uint64_t created_ms;
    uint64_t updated_ms;
    uint32_t reboot_delay_ms;
    bool has_result;
    char result_json[ZGW_JOB_RESULT_MAX_LEN];
} zgw_job_slot_t;

int job_queue_find_slot_index_by_id(const zgw_job_slot_t *jobs, uint32_t id);
int job_queue_alloc_slot_index(zgw_job_slot_t *jobs, const char *tag);
int job_queue_find_inflight_slot_index(const zgw_job_slot_t *jobs, zgw_job_type_t type, uint32_t reboot_delay_ms);
void job_queue_prune_completed_jobs(zgw_job_slot_t *jobs, uint64_t now_ms);
uint32_t job_queue_inflight_depth(const zgw_job_slot_t *jobs);

void job_queue_push_latency_sample(uint32_t *samples, size_t *count, size_t *next, uint32_t latency_ms);
uint32_t job_queue_latency_p95(const uint32_t *samples, size_t count);

void job_queue_set_result(zgw_job_slot_t *job, const char *json_data);
uint64_t job_queue_now_ms(void);
