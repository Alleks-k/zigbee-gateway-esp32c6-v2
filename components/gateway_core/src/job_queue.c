#include "job_queue.h"
#include "wifi_service.h"
#include "system_service.h"
#include "settings_manager.h"
#include "rcp_tool.h"
#include "zigbee_service.h"
#include "gateway_events.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "JOB_QUEUE";

#define ZGW_JOB_MAX 12

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

static SemaphoreHandle_t s_mutex = NULL;
static QueueHandle_t s_job_q = NULL;
static TaskHandle_t s_worker = NULL;
static zgw_job_slot_t s_jobs[ZGW_JOB_MAX];
static uint32_t s_next_id = 1;

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

const char *job_queue_type_to_string(zgw_job_type_t type)
{
    switch (type) {
    case ZGW_JOB_TYPE_WIFI_SCAN: return "scan";
    case ZGW_JOB_TYPE_FACTORY_RESET: return "factory_reset";
    case ZGW_JOB_TYPE_REBOOT: return "reboot";
    case ZGW_JOB_TYPE_UPDATE: return "update";
    case ZGW_JOB_TYPE_LQI_REFRESH: return "lqi_refresh";
    default: return "unknown";
    }
}

const char *job_queue_state_to_string(zgw_job_state_t state)
{
    switch (state) {
    case ZGW_JOB_STATE_QUEUED: return "queued";
    case ZGW_JOB_STATE_RUNNING: return "running";
    case ZGW_JOB_STATE_SUCCEEDED: return "succeeded";
    case ZGW_JOB_STATE_FAILED: return "failed";
    default: return "unknown";
    }
}

static int find_slot_index_by_id(uint32_t id)
{
    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (s_jobs[i].used && s_jobs[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int alloc_slot_index(void)
{
    for (int i = 0; i < ZGW_JOB_MAX; i++) {
        if (!s_jobs[i].used) {
            return i;
        }
    }
    return -1;
}

static void set_job_result(zgw_job_slot_t *job, const char *json_data)
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

static esp_err_t build_scan_result_json(char *out, size_t out_size)
{
    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t err = wifi_service_scan(&list, &count);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        cJSON_Delete(root);
        cJSON_Delete(arr);
        wifi_service_scan_free(list);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "count", (double)count);
    cJSON_AddItemToObject(root, "networks", arr);

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            wifi_service_scan_free(list);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddStringToObject(item, "ssid", list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", list[i].rssi);
        cJSON_AddNumberToObject(item, "auth", list[i].auth);
        cJSON_AddItemToArray(arr, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    wifi_service_scan_free(list);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    int written = snprintf(out, out_size, "%s", json);
    free(json);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t build_factory_reset_result_json(char *out, size_t out_size)
{
    esp_err_t err = settings_manager_factory_reset();
    if (err != ESP_OK) {
        return err;
    }

    settings_manager_factory_reset_report_t report = {0};
    err = settings_manager_get_last_factory_reset_report(&report);
    if (err != ESP_OK) {
        return err;
    }

    int written = snprintf(
        out, out_size,
        "{\"message\":\"Factory reset completed\",\"details\":{\"wifi\":\"%s\",\"devices\":\"%s\","
        "\"zigbee_storage\":\"%s\",\"zigbee_fct\":\"%s\"}}",
        esp_err_to_name(report.wifi_err),
        esp_err_to_name(report.devices_err),
        esp_err_to_name(report.zigbee_storage_err),
        esp_err_to_name(report.zigbee_fct_err));
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t build_reboot_result_json(uint32_t delay_ms, char *out, size_t out_size)
{
    esp_err_t err = system_service_schedule_reboot(delay_ms);
    if (err != ESP_OK) {
        return err;
    }
    int written = snprintf(out, out_size, "{\"message\":\"Reboot scheduled\",\"delay_ms\":%" PRIu32 "}", delay_ms);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t build_update_result_json(char *out, size_t out_size)
{
#if CONFIG_OPENTHREAD_SPINEL_ONLY
    esp_err_t err = check_ot_rcp_version();
    if (err != ESP_OK) {
        return err;
    }
    int written = snprintf(out, out_size, "{\"message\":\"Update check completed\"}");
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#else
    (void)out;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static const char *lqi_quality_label_from_value(int lqi, int rssi)
{
    (void)rssi;
    if (lqi <= 0) {
        return "unknown";
    }
    if (lqi >= 180) {
        return "good";
    }
    if (lqi >= 120) {
        return "warn";
    }
    return "bad";
}

static esp_err_t build_lqi_refresh_result_json(char *out, size_t out_size)
{
    zigbee_neighbor_lqi_t neighbors[MAX_DEVICES] = {0};
    int count = 0;
    esp_err_t err = zigbee_service_refresh_neighbor_lqi_snapshot(neighbors, MAX_DEVICES, &count);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        cJSON_Delete(root);
        cJSON_Delete(arr);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddItemToObject(root, "neighbors", arr);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(item, "short_addr", neighbors[i].short_addr);
        if (neighbors[i].lqi <= 0) {
            cJSON_AddNullToObject(item, "lqi");
        } else {
            cJSON_AddNumberToObject(item, "lqi", neighbors[i].lqi);
        }

        if (neighbors[i].rssi == 127 || neighbors[i].rssi <= -127) {
            cJSON_AddNullToObject(item, "rssi");
        } else {
            cJSON_AddNumberToObject(item, "rssi", neighbors[i].rssi);
        }
        cJSON_AddStringToObject(item, "quality", lqi_quality_label_from_value(neighbors[i].lqi, neighbors[i].rssi));
        cJSON_AddItemToArray(arr, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    int written = snprintf(out, out_size, "%s", json);
    free(json);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void execute_job(uint32_t job_id)
{
    zgw_job_type_t type = ZGW_JOB_TYPE_WIFI_SCAN;
    uint32_t reboot_delay_ms = 1000;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int idx = find_slot_index_by_id(job_id);
    if (idx < 0 || !s_jobs[idx].used) {
        xSemaphoreGive(s_mutex);
        return;
    }
    s_jobs[idx].state = ZGW_JOB_STATE_RUNNING;
    s_jobs[idx].updated_ms = now_ms();
    type = s_jobs[idx].type;
    reboot_delay_ms = s_jobs[idx].reboot_delay_ms;
    xSemaphoreGive(s_mutex);

    char result[ZGW_JOB_RESULT_MAX_LEN] = {0};
    esp_err_t exec_err = ESP_ERR_NOT_SUPPORTED;
    switch (type) {
    case ZGW_JOB_TYPE_WIFI_SCAN:
        exec_err = build_scan_result_json(result, sizeof(result));
        break;
    case ZGW_JOB_TYPE_FACTORY_RESET:
        exec_err = build_factory_reset_result_json(result, sizeof(result));
        break;
    case ZGW_JOB_TYPE_REBOOT:
        exec_err = build_reboot_result_json(reboot_delay_ms, result, sizeof(result));
        break;
    case ZGW_JOB_TYPE_UPDATE:
        exec_err = build_update_result_json(result, sizeof(result));
        break;
    case ZGW_JOB_TYPE_LQI_REFRESH:
        exec_err = build_lqi_refresh_result_json(result, sizeof(result));
        break;
    default:
        exec_err = ESP_ERR_INVALID_ARG;
        break;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    idx = find_slot_index_by_id(job_id);
    if (idx >= 0 && s_jobs[idx].used) {
        s_jobs[idx].err = exec_err;
        s_jobs[idx].state = (exec_err == ESP_OK) ? ZGW_JOB_STATE_SUCCEEDED : ZGW_JOB_STATE_FAILED;
        s_jobs[idx].updated_ms = now_ms();
        if (exec_err == ESP_OK) {
            set_job_result(&s_jobs[idx], result);
        } else {
            char fail_json[128];
            int written = snprintf(fail_json, sizeof(fail_json),
                                   "{\"error\":\"%s\"}", esp_err_to_name(exec_err));
            if (written > 0 && (size_t)written < sizeof(fail_json)) {
                set_job_result(&s_jobs[idx], fail_json);
            }
        }
    }
    xSemaphoreGive(s_mutex);

    if (exec_err == ESP_OK && type == ZGW_JOB_TYPE_LQI_REFRESH) {
        esp_err_t post_ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, NULL, 0, 0);
        if (post_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post LQI_STATE_CHANGED: %s", esp_err_to_name(post_ret));
        }
    }
}

static void job_worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t id = 0;
        if (xQueueReceive(s_job_q, &id, portMAX_DELAY) == pdTRUE) {
            execute_job(id);
        }
    }
}

esp_err_t job_queue_init(void)
{
    if (s_mutex && s_job_q && s_worker) {
        return ESP_OK;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_job_q) {
        s_job_q = xQueueCreate(ZGW_JOB_MAX, sizeof(uint32_t));
        if (!s_job_q) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_worker) {
        BaseType_t ok = xTaskCreate(job_worker_task, "zgw_jobs", 6144, NULL, 5, &s_worker);
        if (ok != pdPASS) {
            s_worker = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t job_queue_submit(zgw_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id)
{
    if (!out_job_id) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int idx = alloc_slot_index();
    if (idx < 0) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    uint32_t id = s_next_id++;
    if (s_next_id == 0) {
        s_next_id = 1;
    }

    memset(&s_jobs[idx], 0, sizeof(s_jobs[idx]));
    s_jobs[idx].used = true;
    s_jobs[idx].id = id;
    s_jobs[idx].type = type;
    s_jobs[idx].state = ZGW_JOB_STATE_QUEUED;
    s_jobs[idx].err = ESP_OK;
    s_jobs[idx].created_ms = now_ms();
    s_jobs[idx].updated_ms = s_jobs[idx].created_ms;
    s_jobs[idx].reboot_delay_ms = reboot_delay_ms;
    xSemaphoreGive(s_mutex);

    if (xQueueSend(s_job_q, &id, 0) != pdTRUE) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_jobs[idx].used = false;
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    *out_job_id = id;
    ESP_LOGI(TAG, "Job queued id=%" PRIu32 " type=%s", id, job_queue_type_to_string(type));
    return ESP_OK;
}

esp_err_t job_queue_get(uint32_t job_id, zgw_job_info_t *out_info)
{
    if (!out_info || job_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int idx = find_slot_index_by_id(job_id);
    if (idx < 0 || !s_jobs[idx].used) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    out_info->id = s_jobs[idx].id;
    out_info->type = s_jobs[idx].type;
    out_info->state = s_jobs[idx].state;
    out_info->err = s_jobs[idx].err;
    out_info->created_ms = s_jobs[idx].created_ms;
    out_info->updated_ms = s_jobs[idx].updated_ms;
    out_info->has_result = s_jobs[idx].has_result;
    strlcpy(out_info->result_json, s_jobs[idx].result_json, sizeof(out_info->result_json));
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
