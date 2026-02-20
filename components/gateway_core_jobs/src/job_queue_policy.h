#pragma once

#include "job_queue.h"

#include <stddef.h>
#include <stdint.h>

esp_err_t job_queue_policy_execute(zgw_job_type_t type, uint32_t reboot_delay_ms, zigbee_service_handle_t zigbee_service_handle,
                                   char *result, size_t result_size);
