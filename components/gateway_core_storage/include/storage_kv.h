#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct storage_kv_handle_s *storage_kv_handle_t;

esp_err_t storage_kv_init(void);

esp_err_t storage_kv_open_readonly(const char *ns, storage_kv_handle_t *out_handle);
esp_err_t storage_kv_open_readwrite(const char *ns, storage_kv_handle_t *out_handle);
void storage_kv_close(storage_kv_handle_t handle);
esp_err_t storage_kv_commit(storage_kv_handle_t handle);

esp_err_t storage_kv_get_i32(storage_kv_handle_t handle, const char *key, int32_t *out_value, bool *out_found);
esp_err_t storage_kv_set_i32(storage_kv_handle_t handle, const char *key, int32_t value);

esp_err_t storage_kv_get_u32(storage_kv_handle_t handle, const char *key, uint32_t *out_value, bool *out_found);
esp_err_t storage_kv_set_u32(storage_kv_handle_t handle, const char *key, uint32_t value);

esp_err_t storage_kv_get_str(storage_kv_handle_t handle, const char *key, char *out_value, size_t out_size,
                             bool *out_found);
esp_err_t storage_kv_set_str(storage_kv_handle_t handle, const char *key, const char *value);

esp_err_t storage_kv_get_blob(storage_kv_handle_t handle, const char *key, void *out_value, size_t out_size,
                              size_t *out_len, bool *out_found);
esp_err_t storage_kv_set_blob(storage_kv_handle_t handle, const char *key, const void *value, size_t value_len);

esp_err_t storage_kv_erase_key(storage_kv_handle_t handle, const char *key, bool *out_existed);
esp_err_t storage_kv_erase_partition(const char *partition_label, bool *out_found);
