#include "ws_manager_state.h"

#include "ws_manager_internal.h"

#include <string.h>

bool ws_manager_metrics_provider(ws_manager_handle_t handle, api_ws_runtime_metrics_t *out_metrics)
{
    if (!handle || !out_metrics) {
        return false;
    }
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    *out_metrics = handle->ws_metrics;
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
    return true;
}

int ws_manager_get_client_count_with_handle(ws_manager_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    int count = 0;
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (handle->ws_fds[i] != -1) {
            count++;
        }
    }
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
    return count;
}

uint32_t ws_manager_client_count_provider(ws_manager_handle_t handle)
{
    int count = ws_manager_get_client_count_with_handle(handle);
    return (count < 0) ? 0u : (uint32_t)count;
}

uint32_t ws_manager_next_seq(ws_manager_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    uint32_t seq = 0;
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    handle->ws_seq++;
    seq = handle->ws_seq;
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
    return seq;
}

bool ws_manager_add_fd(ws_manager_handle_t handle, int fd)
{
    if (!handle) {
        return false;
    }
    bool added = false;
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (handle->ws_fds[i] == fd) {
            added = true;
            break;
        }
    }
    for (int i = 0; i < MAX_WS_CLIENTS && !added; i++) {
        if (handle->ws_fds[i] == -1) {
            handle->ws_fds[i] = fd;
            added = true;
            break;
        }
    }
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
    return added;
}

void ws_manager_remove_fd_internal(ws_manager_handle_t handle, int fd)
{
    if (!handle) {
        return;
    }

    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (handle->ws_fds[i] == fd) {
            handle->ws_fds[i] = -1;
            break;
        }
    }
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }

    if (ws_manager_get_client_count_with_handle(handle) == 0 && handle->ws_periodic_timer) {
        (void)esp_timer_stop(handle->ws_periodic_timer);
    }
}

void ws_manager_note_connection(ws_manager_handle_t handle)
{
    if (!handle) {
        return;
    }
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    handle->ws_metrics.connections_total++;
    if (handle->ws_metrics.connections_total > 1) {
        handle->ws_metrics.reconnect_count++;
    }
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
}

void ws_manager_inc_dropped_frames(ws_manager_handle_t handle)
{
    if (!handle) {
        return;
    }
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    handle->ws_metrics.dropped_frames_total++;
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
}

void ws_manager_inc_lock_skips(ws_manager_handle_t handle)
{
    if (!handle) {
        return;
    }
    if (handle->ws_mutex) {
        xSemaphoreTake(handle->ws_mutex, portMAX_DELAY);
    }
    handle->ws_metrics.broadcast_lock_skips_total++;
    if (handle->ws_mutex) {
        xSemaphoreGive(handle->ws_mutex);
    }
}
