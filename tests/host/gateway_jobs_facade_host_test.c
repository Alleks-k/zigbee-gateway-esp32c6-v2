#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "gateway_jobs_facade.h"
#include "job_queue.h"

static int g_job_queue_create_calls = 0;
static int g_job_queue_init_calls = 0;
static int g_job_queue_destroy_calls = 0;
static int g_job_queue_set_zigbee_calls = 0;
static int g_job_queue_get_metrics_calls = 0;
static int g_job_queue_submit_calls = 0;
static int g_job_queue_get_calls = 0;

static job_queue_handle_t g_created_queue = (job_queue_handle_t)(uintptr_t)0x1111;
static zigbee_service_handle_t g_last_zigbee_handle = NULL;

static void reset_stubs(void)
{
    g_job_queue_create_calls = 0;
    g_job_queue_init_calls = 0;
    g_job_queue_destroy_calls = 0;
    g_job_queue_set_zigbee_calls = 0;
    g_job_queue_get_metrics_calls = 0;
    g_job_queue_submit_calls = 0;
    g_job_queue_get_calls = 0;
    g_last_zigbee_handle = NULL;
}

esp_err_t job_queue_create(job_queue_handle_t *out_handle)
{
    g_job_queue_create_calls++;
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = g_created_queue;
    return ESP_OK;
}

void job_queue_destroy(job_queue_handle_t handle)
{
    if (handle) {
        g_job_queue_destroy_calls++;
    }
}

esp_err_t job_queue_init_with_handle(job_queue_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    g_job_queue_init_calls++;
    return ESP_OK;
}

esp_err_t job_queue_set_zigbee_service_with_handle(job_queue_handle_t handle, zigbee_service_handle_t zigbee_service_handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    g_job_queue_set_zigbee_calls++;
    g_last_zigbee_handle = zigbee_service_handle;
    return ESP_OK;
}

esp_err_t job_queue_submit_with_handle(job_queue_handle_t handle, zgw_job_type_t type, uint32_t reboot_delay_ms,
                                       uint32_t *out_job_id)
{
    (void)type;
    (void)reboot_delay_ms;
    g_job_queue_submit_calls++;
    if (!handle || !out_job_id) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_job_id = 77;
    return ESP_OK;
}

esp_err_t job_queue_get_with_handle(job_queue_handle_t handle, uint32_t job_id, zgw_job_info_t *out_info)
{
    (void)job_id;
    g_job_queue_get_calls++;
    if (!handle || !out_info) {
        return ESP_ERR_INVALID_ARG;
    }

    out_info->id = 77;
    out_info->type = ZGW_JOB_TYPE_REBOOT;
    out_info->state = ZGW_JOB_STATE_SUCCEEDED;
    out_info->err = ESP_OK;
    out_info->created_ms = 100;
    out_info->updated_ms = 200;
    out_info->has_result = true;
    out_info->result_json[0] = '{';
    out_info->result_json[1] = '}';
    out_info->result_json[2] = '\0';
    return ESP_OK;
}

esp_err_t job_queue_get_metrics_with_handle(job_queue_handle_t handle, zgw_job_metrics_t *out_metrics)
{
    g_job_queue_get_metrics_calls++;
    if (!handle || !out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }

    out_metrics->submitted_total = 1;
    out_metrics->dedup_reused_total = 2;
    out_metrics->completed_total = 3;
    out_metrics->failed_total = 4;
    out_metrics->queue_depth_current = 5;
    out_metrics->queue_depth_peak = 6;
    out_metrics->latency_p95_ms = 7;
    return ESP_OK;
}

const char *job_queue_type_to_string(zgw_job_type_t type)
{
    (void)type;
    return "ok";
}

const char *job_queue_state_to_string(zgw_job_state_t state)
{
    (void)state;
    return "ok";
}

static void test_create_with_external_queue_does_not_own_queue(void)
{
    reset_stubs();

    gateway_jobs_handle_t jobs = NULL;
    gateway_jobs_init_params_t params = {
        .job_queue_handle = (job_queue_handle_t)(uintptr_t)0x2222,
    };

    assert(gateway_jobs_create(&params, &jobs) == ESP_OK);
    assert(jobs != NULL);
    assert(g_job_queue_create_calls == 0);
    assert(g_job_queue_init_calls == 1);

    gateway_jobs_destroy(jobs);
    assert(g_job_queue_destroy_calls == 0);
}

static void test_create_without_queue_owns_queue_and_forwards_ops(void)
{
    reset_stubs();

    gateway_jobs_handle_t jobs = NULL;
    assert(gateway_jobs_create(NULL, &jobs) == ESP_OK);
    assert(jobs != NULL);
    assert(g_job_queue_create_calls == 1);
    assert(g_job_queue_init_calls == 1);

    struct zigbee_service *zigbee = (struct zigbee_service *)(uintptr_t)0x3333;
    assert(gateway_jobs_set_zigbee_service(jobs, zigbee) == ESP_OK);
    assert(g_job_queue_set_zigbee_calls == 1);
    assert(g_last_zigbee_handle == zigbee);

    gateway_core_job_metrics_t metrics = {0};
    assert(gateway_jobs_get_metrics(jobs, &metrics) == ESP_OK);
    assert(g_job_queue_get_metrics_calls == 1);
    assert(metrics.submitted_total == 1);
    assert(metrics.latency_p95_ms == 7);

    uint32_t job_id = 0;
    assert(gateway_jobs_submit(jobs, GATEWAY_CORE_JOB_TYPE_REBOOT, 1200, &job_id) == ESP_OK);
    assert(g_job_queue_submit_calls == 1);
    assert(job_id == 77);

    gateway_core_job_info_t info = {0};
    assert(gateway_jobs_get(jobs, 77, &info) == ESP_OK);
    assert(g_job_queue_get_calls == 1);
    assert(info.id == 77);
    assert(info.type == GATEWAY_CORE_JOB_TYPE_REBOOT);
    assert(info.state == GATEWAY_CORE_JOB_STATE_SUCCEEDED);

    gateway_jobs_destroy(jobs);
    assert(g_job_queue_destroy_calls == 1);
}

int main(void)
{
    printf("Running host tests: gateway_jobs_facade_host_test\n");

    test_create_with_external_queue_does_not_own_queue();
    test_create_without_queue_owns_queue_and_forwards_ops();

    printf("Host tests passed: gateway_jobs_facade_host_test\n");
    return 0;
}
