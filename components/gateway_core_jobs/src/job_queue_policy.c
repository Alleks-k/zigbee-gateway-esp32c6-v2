#include "job_queue_policy.h"

#include "job_queue_json.h"

esp_err_t job_queue_policy_execute(zgw_job_type_t type,
                                   uint32_t reboot_delay_ms,
                                   zigbee_service_handle_t zigbee_service_handle,
                                   struct wifi_service *wifi_service_handle,
                                   struct system_service *system_service_handle,
                                   char *result,
                                   size_t result_size)
{
    switch (type) {
    case ZGW_JOB_TYPE_WIFI_SCAN:
        return job_queue_json_build_scan_result_with_services(wifi_service_handle, result, result_size);
    case ZGW_JOB_TYPE_FACTORY_RESET:
        return job_queue_json_build_factory_reset_result(result, result_size);
    case ZGW_JOB_TYPE_REBOOT:
        return job_queue_json_build_reboot_result_with_services(system_service_handle, reboot_delay_ms, result, result_size);
    case ZGW_JOB_TYPE_UPDATE:
        return job_queue_json_build_update_result(result, result_size);
    case ZGW_JOB_TYPE_LQI_REFRESH:
        return job_queue_json_build_lqi_refresh_result(zigbee_service_handle, result, result_size);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}
