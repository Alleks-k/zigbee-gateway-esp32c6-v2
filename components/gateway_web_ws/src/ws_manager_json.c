#include "ws_manager_json.h"

#include "ws_manager_internal.h"
#include "ws_manager_state.h"

#include "esp_timer.h"

#include <inttypes.h>
#include <stdio.h>

esp_err_t ws_manager_wrap_event_payload(const char *type, const char *data_json, size_t data_len, size_t *out_len)
{
    if (!type || !data_json || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t seq = ws_manager_next_seq();
    uint64_t ts_ms = (uint64_t)(esp_timer_get_time() / 1000);
    int written = snprintf(s_ws_frame_buf, sizeof(s_ws_frame_buf),
                           "{\"version\":%d,\"seq\":%" PRIu32 ",\"ts\":%" PRIu64 ",\"type\":\"%s\",\"data\":%.*s}",
                           WS_PROTOCOL_VERSION, seq, ts_ms, type, (int)data_len, data_json);
    if (written < 0 || (size_t)written >= sizeof(s_ws_frame_buf)) {
        return ESP_ERR_NO_MEM;
    }
    *out_len = (size_t)written;
    return ESP_OK;
}
