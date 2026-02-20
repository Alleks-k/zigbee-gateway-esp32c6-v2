#include "zigbee_service.h"

#include <stdlib.h>
#include <string.h>

#include "gateway_status_esp.h"
#include "esp_timer.h"
#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nwk/esp_zigbee_nwk.h"
#include "zdo/esp_zigbee_zdo_command.h"

struct zigbee_service {
    device_service_handle_t device_service;
    gateway_state_handle_t gateway_state;
    const zigbee_service_runtime_ops_t *runtime_ops;
};

static esp_err_t runtime_send_on_off_not_supported(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    (void)short_addr;
    (void)endpoint;
    (void)on_off;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t runtime_delete_device_not_supported(uint16_t short_addr)
{
    (void)short_addr;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t runtime_rename_device_not_supported(uint16_t short_addr, const char *name)
{
    (void)short_addr;
    (void)name;
    return ESP_ERR_NOT_SUPPORTED;
}

static const zigbee_service_runtime_ops_t s_default_runtime_ops = {
    .send_on_off = runtime_send_on_off_not_supported,
    .delete_device = runtime_delete_device_not_supported,
    .rename_device = runtime_rename_device_not_supported,
};

static bool service_ready(zigbee_service_handle_t handle)
{
    return handle && handle->device_service && handle->gateway_state;
}

esp_err_t zigbee_service_create(const zigbee_service_init_params_t *params, zigbee_service_handle_t *out_handle)
{
    if (!params || !params->device_service || !params->gateway_state || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    zigbee_service_t *handle = (zigbee_service_t *)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    handle->device_service = params->device_service;
    handle->gateway_state = params->gateway_state;
    handle->runtime_ops = params->runtime_ops ? params->runtime_ops : &s_default_runtime_ops;
    *out_handle = handle;
    return ESP_OK;
}

void zigbee_service_destroy(zigbee_service_handle_t handle)
{
    free(handle);
}

static gateway_lqi_source_t to_gateway_lqi_source(zigbee_lqi_source_t src)
{
    switch (src) {
    case ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE:
        return GATEWAY_LQI_SOURCE_NEIGHBOR_TABLE;
    case ZIGBEE_LQI_SOURCE_MGMT_LQI:
        return GATEWAY_LQI_SOURCE_MGMT_LQI;
    case ZIGBEE_LQI_SOURCE_UNKNOWN:
    default:
        return GATEWAY_LQI_SOURCE_UNKNOWN;
    }
}

static zigbee_lqi_source_t from_gateway_lqi_source(gateway_lqi_source_t src)
{
    switch (src) {
    case GATEWAY_LQI_SOURCE_NEIGHBOR_TABLE:
        return ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE;
    case GATEWAY_LQI_SOURCE_MGMT_LQI:
        return ZIGBEE_LQI_SOURCE_MGMT_LQI;
    case GATEWAY_LQI_SOURCE_UNKNOWN:
    default:
        return ZIGBEE_LQI_SOURCE_UNKNOWN;
    }
}

static void update_gateway_lqi_from_snapshot(zigbee_service_handle_t handle, const zigbee_neighbor_lqi_t *items, int count,
                                             zigbee_lqi_source_t source)
{
    if (!service_ready(handle) || !items || count <= 0) {
        return;
    }

    uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000);
    gateway_lqi_source_t gw_src = to_gateway_lqi_source(source);
    for (int i = 0; i < count; i++) {
        uint64_t ts = items[i].updated_ms > 0 ? items[i].updated_ms : now_ms;
        (void)gateway_state_update_lqi(handle->gateway_state, items[i].short_addr, items[i].lqi, items[i].rssi, gw_src, ts);
    }
}

esp_err_t zigbee_service_get_network_status(zigbee_service_handle_t handle, zigbee_network_status_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!service_ready(handle)) {
        return ESP_ERR_INVALID_STATE;
    }

    gateway_network_state_t state = {0};
    esp_err_t ret = gateway_status_to_esp_err(gateway_state_get_network(handle->gateway_state, &state));
    if (ret != ESP_OK) {
        return ret;
    }
    out->pan_id = state.pan_id;
    out->channel = state.channel;
    out->short_addr = state.short_addr;
    return ESP_OK;
}

esp_err_t zigbee_service_permit_join(zigbee_service_handle_t handle, uint16_t seconds)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_zb_bdb_open_network(seconds);
    return ESP_OK;
}

esp_err_t zigbee_service_send_on_off(zigbee_service_handle_t handle, uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    if (!handle || !handle->runtime_ops || !handle->runtime_ops->send_on_off) {
        return ESP_ERR_INVALID_STATE;
    }
    return handle->runtime_ops->send_on_off(short_addr, endpoint, on_off);
}

int zigbee_service_get_devices_snapshot(zigbee_service_handle_t handle, zb_device_t *out, size_t max_items)
{
    if (!service_ready(handle)) {
        return 0;
    }
    return device_service_get_snapshot(handle->device_service, out, max_items);
}

int zigbee_service_get_neighbor_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }
    if (!service_ready(handle)) {
        return 0;
    }

    gateway_network_state_t state = {0};
    if (gateway_status_to_esp_err(gateway_state_get_network(handle->gateway_state, &state)) != ESP_OK || !state.zigbee_started) {
        return 0;
    }

    esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    int count = 0;
    uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000);
    while ((size_t)count < max_items) {
        esp_zb_nwk_neighbor_info_t info = {0};
        esp_err_t ret = esp_zb_nwk_get_next_neighbor(&it, &info);
        if (ret != ESP_OK) {
            break;
        }

        out[count].short_addr = info.short_addr;
        out[count].lqi = (int)info.lqi;
        out[count].rssi = (int)info.rssi;
        out[count].relationship = info.relationship;
        out[count].depth = info.depth;
        out[count].updated_ms = now_ms;
        out[count].source = ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE;
        count++;
    }

    update_gateway_lqi_from_snapshot(handle, out, count, ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE);
    return count;
}

typedef struct {
    SemaphoreHandle_t done_sem;
    zigbee_neighbor_lqi_t *out;
    size_t max_items;
    size_t count;
    uint8_t next_start_index;
    uint8_t total_entries;
    esp_err_t err;
} lqi_req_ctx_t;

static void mgmt_lqi_rsp_cb(const esp_zb_zdo_mgmt_lqi_rsp_t *rsp, void *user_ctx)
{
    lqi_req_ctx_t *ctx = (lqi_req_ctx_t *)user_ctx;
    if (!ctx) {
        return;
    }

    if (!rsp || rsp->status != 0) {
        ctx->err = ESP_FAIL;
        if (ctx->done_sem) {
            xSemaphoreGive(ctx->done_sem);
        }
        return;
    }

    ctx->total_entries = rsp->neighbor_table_entries;
    for (uint8_t i = 0; i < rsp->neighbor_table_list_count && ctx->count < ctx->max_items; i++) {
        const esp_zb_zdo_neighbor_table_list_record_t *rec = &rsp->neighbor_table_list[i];
        ctx->out[ctx->count].short_addr = rec->network_addr;
        ctx->out[ctx->count].lqi = (int)rec->lqi;
        ctx->out[ctx->count].rssi = 127;
        ctx->out[ctx->count].relationship = rec->relationship;
        ctx->out[ctx->count].depth = rec->depth;
        ctx->out[ctx->count].updated_ms = (uint64_t)(esp_timer_get_time() / 1000);
        ctx->out[ctx->count].source = ZIGBEE_LQI_SOURCE_MGMT_LQI;
        ctx->count++;
    }

    ctx->next_start_index = (uint8_t)(rsp->start_index + rsp->neighbor_table_list_count);
    ctx->err = ESP_OK;
    if (ctx->done_sem) {
        xSemaphoreGive(ctx->done_sem);
    }
}

esp_err_t zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out,
                                                       size_t max_items, int *out_count)
{
    if (!out || max_items == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0;
    if (!service_ready(handle)) {
        return ESP_ERR_INVALID_STATE;
    }

    gateway_network_state_t state = {0};
    esp_err_t sret = gateway_status_to_esp_err(gateway_state_get_network(handle->gateway_state, &state));
    if (sret != ESP_OK || !state.zigbee_started) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(out, 0, sizeof(zigbee_neighbor_lqi_t) * max_items);

    lqi_req_ctx_t ctx = {
        .done_sem = xSemaphoreCreateBinary(),
        .out = out,
        .max_items = max_items,
        .count = 0,
        .next_start_index = 0,
        .total_entries = 0,
        .err = ESP_FAIL,
    };
    if (!ctx.done_sem) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    uint8_t pages = 0;
    do {
        esp_zb_zdo_mgmt_lqi_req_param_t req = {
            .start_index = ctx.next_start_index,
            .dst_addr = (uint16_t)state.short_addr,
        };

        if (!esp_zb_lock_acquire(pdMS_TO_TICKS(2000))) {
            ret = ESP_ERR_TIMEOUT;
            break;
        }
        esp_zb_zdo_mgmt_lqi_req(&req, mgmt_lqi_rsp_cb, &ctx);
        esp_zb_lock_release();

        if (xSemaphoreTake(ctx.done_sem, pdMS_TO_TICKS(3500)) != pdTRUE) {
            ret = ESP_ERR_TIMEOUT;
            break;
        }
        if (ctx.err != ESP_OK) {
            ret = ctx.err;
            break;
        }
        pages++;
    } while (ctx.next_start_index < ctx.total_entries && ctx.count < max_items && pages < 8);

    if (ret == ESP_OK) {
        *out_count = (int)ctx.count;
        update_gateway_lqi_from_snapshot(handle, out, *out_count, ZIGBEE_LQI_SOURCE_MGMT_LQI);
    }

    vSemaphoreDelete(ctx.done_sem);
    return ret;
}

esp_err_t zigbee_service_get_cached_lqi_snapshot(zigbee_service_handle_t handle, zigbee_neighbor_lqi_t *out,
                                                 size_t max_items, int *out_count, zigbee_lqi_source_t *out_source,
                                                 uint64_t *out_updated_ms)
{
    if (!out || max_items == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!service_ready(handle)) {
        return ESP_ERR_INVALID_STATE;
    }

    gateway_lqi_cache_entry_t snapshot[MAX_DEVICES];
    int count = gateway_state_get_lqi_snapshot(handle->gateway_state, snapshot, MAX_DEVICES);
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }

    zigbee_lqi_source_t latest_source = ZIGBEE_LQI_SOURCE_UNKNOWN;
    uint64_t latest_ts = 0;
    for (int i = 0; i < count; i++) {
        out[i].short_addr = snapshot[i].short_addr;
        out[i].lqi = snapshot[i].lqi;
        out[i].rssi = snapshot[i].rssi;
        out[i].relationship = 0;
        out[i].depth = 0;
        out[i].updated_ms = snapshot[i].updated_ms;
        out[i].source = from_gateway_lqi_source(snapshot[i].source);
        if (snapshot[i].updated_ms >= latest_ts) {
            latest_ts = snapshot[i].updated_ms;
            latest_source = out[i].source;
        }
    }

    *out_count = count;
    if (out_source) {
        *out_source = latest_source;
    }
    if (out_updated_ms) {
        *out_updated_ms = latest_ts;
    }

    return ESP_OK;
}

esp_err_t zigbee_service_refresh_neighbor_lqi_from_table(zigbee_service_handle_t handle)
{
    zigbee_neighbor_lqi_t neighbors[MAX_DEVICES] = {0};
    (void)zigbee_service_get_neighbor_lqi_snapshot(handle, neighbors, MAX_DEVICES);
    return ESP_OK;
}

esp_err_t zigbee_service_delete_device(zigbee_service_handle_t handle, uint16_t short_addr)
{
    if (!handle || !handle->runtime_ops || !handle->runtime_ops->delete_device) {
        return ESP_ERR_INVALID_STATE;
    }
    return handle->runtime_ops->delete_device(short_addr);
}

esp_err_t zigbee_service_rename_device(zigbee_service_handle_t handle, uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!handle || !handle->runtime_ops || !handle->runtime_ops->rename_device) {
        return ESP_ERR_INVALID_STATE;
    }
    return handle->runtime_ops->rename_device(short_addr, name);
}
