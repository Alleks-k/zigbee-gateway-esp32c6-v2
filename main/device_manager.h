#pragma once

#include "esp_zigbee_core.h"
#include "cJSON.h"

#define MAX_DEVICES 10

typedef struct {
    uint16_t short_addr;
    esp_zb_ieee_addr_t ieee_addr; 
    char name[32]; 
} zb_device_t;

/**
 * @brief Ініціалізація менеджера пристроїв (завантаження з NVS)
 */
void device_manager_init(void);

/**
 * @brief Додавання або оновлення пристрою
 */
void add_device_with_ieee(uint16_t addr, esp_zb_ieee_addr_t ieee);

/**
 * @brief Видалення пристрою
 */
void delete_device(uint16_t addr);

/**
 * @brief Отримання списку пристроїв у форматі cJSON Array
 * @note Викликаючий код повинен звільнити пам'ять (cJSON_Delete) для батьківського об'єкта
 */
cJSON* device_manager_get_json_list(void);
