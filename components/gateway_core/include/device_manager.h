#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "gateway_config_types.h"

typedef struct cJSON cJSON;

/* Backward-compatible alias for existing code paths. */
#ifndef MAX_DEVICES
#define MAX_DEVICES GATEWAY_MAX_DEVICES
#endif

typedef gateway_device_record_t zb_device_t;

/**
 * @brief Ініціалізація менеджера пристроїв (завантаження з NVS)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t device_manager_init(void);

/**
 * @brief Додавання або оновлення пристрою
 */
void add_device_with_ieee(uint16_t addr, gateway_ieee_addr_t ieee);

/**
 * @brief Оновлення назви пристрою
 */
void update_device_name(uint16_t addr, const char *new_name);

/**
 * @brief Видалення пристрою
 */
void delete_device(uint16_t addr);

/**
 * @brief Отримання списку пристроїв у форматі cJSON Array
 * @note Викликаючий код повинен звільнити пам'ять (cJSON_Delete) для батьківського об'єкта
 */
cJSON* device_manager_get_json_list(void);

/**
 * @brief Copy current devices to caller-provided buffer.
 *
 * @param out Array buffer to fill.
 * @param max_items Max number of elements available in @p out.
 * @return int Number of copied items.
 */
int device_manager_get_snapshot(zb_device_t *out, size_t max_items);

/**
 * @brief Thread-safe lock for device manager operations
 * Корисно, якщо ви плануєте складні операції читання/запису з різних задач
 */
void device_manager_lock(void);
void device_manager_unlock(void);
