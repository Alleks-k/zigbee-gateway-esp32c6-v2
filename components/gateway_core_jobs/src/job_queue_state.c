#include "job_queue_state.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <inttypes.h>
#include <string.h>

uint64_t job_queue_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

int job_queue_find_slot_index_by_id(const zgw_job_slot_t *jobs, uint32_t id)
{
    if (!jobs) {
        return -1;
    }

    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (jobs[i].used && jobs[i].id == id) {
            return i;
        }
    }
    return -1;
}

int job_queue_alloc_slot_index(zgw_job_slot_t *jobs, const char *tag)
{
    if (!jobs) {
        return -1;
    }

    int reclaim_idx = -1;
    uint64_t reclaim_updated_ms = UINT64_MAX;
    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (!jobs[i].used) {
            return i;
        }
        if (jobs[i].state == ZGW_JOB_STATE_SUCCEEDED || jobs[i].state == ZGW_JOB_STATE_FAILED) {
            if (jobs[i].updated_ms <= reclaim_updated_ms) {
                reclaim_updated_ms = jobs[i].updated_ms;
                reclaim_idx = i;
            }
        }
    }
    if (reclaim_idx >= 0) {
        ESP_LOGW(tag, "Job slots full, evicting completed job id=%" PRIu32, jobs[reclaim_idx].id);
        memset(&jobs[reclaim_idx], 0, sizeof(jobs[reclaim_idx]));
        return reclaim_idx;
    }
    return -1;
}

int job_queue_find_inflight_slot_index(const zgw_job_slot_t *jobs, zgw_job_type_t type, uint32_t reboot_delay_ms)
{
    if (!jobs) {
        return -1;
    }

    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (!jobs[i].used) {
            continue;
        }
        if (jobs[i].type != type) {
            continue;
        }
        if (jobs[i].state != ZGW_JOB_STATE_QUEUED && jobs[i].state != ZGW_JOB_STATE_RUNNING) {
            continue;
        }
        if (type == ZGW_JOB_TYPE_REBOOT && jobs[i].reboot_delay_ms != reboot_delay_ms) {
            continue;
        }
        return i;
    }
    return -1;
}

void job_queue_prune_completed_jobs(zgw_job_slot_t *jobs, uint64_t now_ms)
{
    if (!jobs) {
        return;
    }

    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (!jobs[i].used) {
            continue;
        }
        if (jobs[i].state != ZGW_JOB_STATE_SUCCEEDED && jobs[i].state != ZGW_JOB_STATE_FAILED) {
            continue;
        }
        if (now_ms < jobs[i].updated_ms) {
            continue;
        }
        if ((now_ms - jobs[i].updated_ms) >= ZGW_JOB_COMPLETED_TTL_MS) {
            memset(&jobs[i], 0, sizeof(jobs[i]));
        }
    }
}

uint32_t job_queue_inflight_depth(const zgw_job_slot_t *jobs)
{
    if (!jobs) {
        return 0;
    }

    uint32_t depth = 0;
    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (!jobs[i].used) {
            continue;
        }
        if (jobs[i].state == ZGW_JOB_STATE_QUEUED || jobs[i].state == ZGW_JOB_STATE_RUNNING) {
            depth++;
        }
    }
    return depth;
}

void job_queue_push_latency_sample(uint32_t *samples, size_t *count, size_t *next, uint32_t latency_ms)
{
    if (!samples || !count || !next) {
        return;
    }

    const size_t cap = 64;
    samples[*next] = latency_ms;
    *next = (*next + 1U) % cap;
    if (*count < cap) {
        (*count)++;
    }
}

uint32_t job_queue_latency_p95(const uint32_t *samples, size_t count)
{
    if (!samples || count == 0) {
        return 0;
    }

    uint32_t sorted[64] = {0};
    size_t n = count;
    if (n > 64) {
        n = 64;
    }
    for (size_t i = 0; i < n; i++) {
        sorted[i] = samples[i];
    }
    for (size_t i = 1; i < n; i++) {
        uint32_t key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > key) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = key;
    }
    size_t idx = (n == 1) ? 0 : ((n * 95 + 99) / 100) - 1;
    if (idx >= n) {
        idx = n - 1;
    }
    return sorted[idx];
}

void job_queue_set_result(zgw_job_slot_t *job, const char *json_data)
{
    if (!job || !json_data) {
        return;
    }
    size_t len = strnlen(json_data, ZGW_JOB_RESULT_MAX_LEN);
    if (len >= ZGW_JOB_RESULT_MAX_LEN) {
        len = ZGW_JOB_RESULT_MAX_LEN - 1;
    }
    memcpy(job->result_json, json_data, len);
    job->result_json[len] = '\0';
    job->has_result = true;
}
