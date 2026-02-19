#pragma once

#include "api_usecases.h"
#include "ws_manager.h"

#include <stdbool.h>
#include <stdint.h>

bool ws_manager_metrics_provider(ws_manager_handle_t handle, api_ws_runtime_metrics_t *out_metrics);
uint32_t ws_manager_client_count_provider(ws_manager_handle_t handle);
uint32_t ws_manager_next_seq(ws_manager_handle_t handle);
int ws_manager_get_client_count_with_handle(ws_manager_handle_t handle);
bool ws_manager_add_fd(ws_manager_handle_t handle, int fd);
void ws_manager_remove_fd_internal(ws_manager_handle_t handle, int fd);
void ws_manager_note_connection(ws_manager_handle_t handle);
void ws_manager_inc_dropped_frames(ws_manager_handle_t handle);
void ws_manager_inc_lock_skips(ws_manager_handle_t handle);
