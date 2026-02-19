#pragma once

#include "esp_err.h"
#include "ws_manager.h"

#include <stddef.h>

esp_err_t ws_manager_wrap_event_payload(ws_manager_handle_t handle, const char *type, const char *data_json, size_t data_len,
                                        size_t *out_len);
