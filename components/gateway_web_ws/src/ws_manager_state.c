#include "ws_manager_state.h"

#include "ws_manager_internal.h"

#include <string.h>

bool ws_manager_metrics_provider(api_ws_runtime_metrics_t *out_metrics)
{
    if (!out_metrics) {
        return false;
    }
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    *out_metrics = s_ws_metrics;
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return true;
}

int ws_manager_get_client_count(void)
{
    int count = 0;
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] != -1) {
            count++;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return count;
}

uint32_t ws_manager_client_count_provider(void)
{
    int count = ws_manager_get_client_count();
    return (count < 0) ? 0u : (uint32_t)count;
}

uint32_t ws_manager_next_seq(void)
{
    uint32_t seq = 0;
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    s_ws_seq++;
    seq = s_ws_seq;
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return seq;
}

bool ws_manager_add_fd(int fd)
{
    bool added = false;
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] == fd) {
            added = true;
            break;
        }
    }
    for (int i = 0; i < MAX_WS_CLIENTS && !added; i++) {
        if (ws_fds[i] == -1) {
            ws_fds[i] = fd;
            added = true;
            break;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
    return added;
}

void ws_manager_remove_fd_internal(int fd)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_fds[i] == fd) {
            ws_fds[i] = -1;
            break;
        }
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }

    if (ws_manager_get_client_count() == 0 && s_ws_periodic_timer) {
        (void)esp_timer_stop(s_ws_periodic_timer);
    }
}

void ws_manager_note_connection(void)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    s_ws_metrics.connections_total++;
    if (s_ws_metrics.connections_total > 1) {
        s_ws_metrics.reconnect_count++;
    }
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
}

void ws_manager_inc_dropped_frames(void)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    s_ws_metrics.dropped_frames_total++;
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
}

void ws_manager_inc_lock_skips(void)
{
    if (s_ws_mutex) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    }
    s_ws_metrics.broadcast_lock_skips_total++;
    if (s_ws_mutex) {
        xSemaphoreGive(s_ws_mutex);
    }
}
