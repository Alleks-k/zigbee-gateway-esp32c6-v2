#include "zigbee_service.h"
#include "gateway_state.h"
#include "esp_zigbee_core.h"
#include "nwk/esp_zigbee_nwk.h"
#include "zdo/esp_zigbee_zdo_command.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

/* Implemented in app/main integration layer */
void send_on_off_command(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
void delete_device(uint16_t short_addr);
void update_device_name(uint16_t short_addr, const char *new_name);

static SemaphoreHandle_t s_lqi_cache_mutex = NULL;
static zigbee_neighbor_lqi_t s_lqi_cache[MAX_DEVICES];
static int s_lqi_cache_count = 0;
static zigbee_lqi_source_t s_lqi_cache_source = ZIGBEE_LQI_SOURCE_UNKNOWN;
static uint64_t s_lqi_cache_updated_ms = 0;

static SemaphoreHandle_t get_lqi_cache_mutex(void)
{
    if (!s_lqi_cache_mutex) {
        s_lqi_cache_mutex = xSemaphoreCreateMutex();
    }
    return s_lqi_cache_mutex;
}

static void cache_lqi_snapshot(const zigbee_neighbor_lqi_t *items, int count, zigbee_lqi_source_t source)
{
    SemaphoreHandle_t mutex = get_lqi_cache_mutex();
    if (!mutex) {
        return;
    }

    if (count < 0) {
        count = 0;
    }
    if (count > MAX_DEVICES) {
        count = MAX_DEVICES;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (count > 0 && items) {
        memcpy(s_lqi_cache, items, (size_t)count * sizeof(zigbee_neighbor_lqi_t));
    }
    s_lqi_cache_count = count;
    s_lqi_cache_source = source;
    s_lqi_cache_updated_ms = (uint64_t)(esp_timer_get_time() / 1000);
    xSemaphoreGive(mutex);
}

esp_err_t zigbee_service_get_network_status(zigbee_network_status_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_network_state_t state = {0};
    esp_err_t ret = gateway_state_get_network(&state);
    if (ret != ESP_OK) {
        return ret;
    }
    out->pan_id = state.pan_id;
    out->channel = state.channel;
    out->short_addr = state.short_addr;
    return ESP_OK;
}

esp_err_t zigbee_service_permit_join(uint16_t seconds)
{
    esp_zb_bdb_open_network(seconds);
    return ESP_OK;
}

esp_err_t zigbee_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    send_on_off_command(short_addr, endpoint, on_off);
    return ESP_OK;
}

int zigbee_service_get_devices_snapshot(zb_device_t *out, size_t max_items)
{
    return gateway_state_get_devices_snapshot(out, max_items);
}

int zigbee_service_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }

    gateway_network_state_t state = {0};
    if (gateway_state_get_network(&state) != ESP_OK || !state.zigbee_started) {
        return 0;
    }

    esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
    int count = 0;
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
        count++;
    }

    cache_lqi_snapshot(out, count, ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE);
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
        ctx->out[ctx->count].rssi = 127; /* Not provided by Mgmt_Lqi_rsp */
        ctx->out[ctx->count].relationship = rec->relationship;
        ctx->out[ctx->count].depth = rec->depth;
        ctx->count++;
    }

    ctx->next_start_index = (uint8_t)(rsp->start_index + rsp->neighbor_table_list_count);
    ctx->err = ESP_OK;
    if (ctx->done_sem) {
        xSemaphoreGive(ctx->done_sem);
    }
}

esp_err_t zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items, int *out_count)
{
    if (!out || max_items == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0;

    gateway_network_state_t state = {0};
    esp_err_t sret = gateway_state_get_network(&state);
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
        cache_lqi_snapshot(out, *out_count, ZIGBEE_LQI_SOURCE_MGMT_LQI);
    }

    vSemaphoreDelete(ctx.done_sem);
    return ret;
}

esp_err_t zigbee_service_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items, int *out_count,
                                                 zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    if (!out || max_items == 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t mutex = get_lqi_cache_mutex();
    if (!mutex) {
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    int count = s_lqi_cache_count;
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, s_lqi_cache, (size_t)count * sizeof(zigbee_neighbor_lqi_t));
    }
    *out_count = count;
    if (out_source) {
        *out_source = s_lqi_cache_source;
    }
    if (out_updated_ms) {
        *out_updated_ms = s_lqi_cache_updated_ms;
    }
    xSemaphoreGive(mutex);

    return ESP_OK;
}

esp_err_t zigbee_service_delete_device(uint16_t short_addr)
{
    delete_device(short_addr);
    return ESP_OK;
}

esp_err_t zigbee_service_rename_device(uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    update_device_name(short_addr, name);
    return ESP_OK;
}
