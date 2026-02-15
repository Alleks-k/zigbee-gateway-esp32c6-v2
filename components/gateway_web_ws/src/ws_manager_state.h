#pragma once

#include "api_usecases.h"

#include <stdbool.h>
#include <stdint.h>

bool ws_manager_metrics_provider(api_ws_runtime_metrics_t *out_metrics);
uint32_t ws_manager_client_count_provider(void);
uint32_t ws_manager_next_seq(void);
bool ws_manager_add_fd(int fd);
void ws_manager_remove_fd_internal(int fd);
void ws_manager_note_connection(void);
void ws_manager_inc_dropped_frames(void);
void ws_manager_inc_lock_skips(void);
