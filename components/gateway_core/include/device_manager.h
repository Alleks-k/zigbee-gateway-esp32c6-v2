#pragma once

#include <stddef.h>
#include <stdint.h>
#include "gateway_types.h"

typedef struct cJSON cJSON;

#define MAX_DEVICES 10

typedef struct {
    uint16_t short_addr;
    gateway_ieee_addr_t ieee_addr;
    char name[32]; 
} zb_device_t;

/**
 * @brief Ініціалізація менеджера пристроїв (завантаження з NVS)
 */
void device_manager_init(void);

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
