#pragma once

#include "esp_err.h"
#include "job_queue.h"

#include <stddef.h>
#include <stdint.h>

esp_err_t job_queue_json_build_scan_result(char *out, size_t out_size);
esp_err_t job_queue_json_build_factory_reset_result(char *out, size_t out_size);
esp_err_t job_queue_json_build_reboot_result(uint32_t delay_ms, char *out, size_t out_size);
esp_err_t job_queue_json_build_update_result(char *out, size_t out_size);
esp_err_t job_queue_json_build_lqi_refresh_result(zigbee_service_handle_t zigbee_service_handle, char *out, size_t out_size);
