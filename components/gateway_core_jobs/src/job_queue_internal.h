#pragma once

#include "job_queue.h"
#include "job_queue_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stddef.h>
#include <stdint.h>

typedef struct zgw_job_queue {
    SemaphoreHandle_t mutex;
    QueueHandle_t job_q;
    TaskHandle_t worker;
    zgw_job_slot_t jobs[ZGW_JOB_MAX];
    uint32_t next_id;
    zgw_job_metrics_t metrics;
    uint32_t latency_samples_ms[64];
    size_t latency_samples_count;
    size_t latency_samples_next;
    zigbee_service_handle_t zigbee_service_handle;
    struct wifi_service *wifi_service_handle;
    struct system_service *system_service_handle;
} zgw_job_queue_t;

void job_queue_worker_task(void *arg);
