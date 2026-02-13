/* main/rcp_tool.h */
#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#define RCP_VERSION_MAX_SIZE 80

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Обробник помилок RCP (Radio Co-Processor)
 */
void rcp_error_handler(void);

#if CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP
/**
 * @brief Повна ініціалізація сервісу автоматичного оновлення RCP
 */
void rcp_init_auto_update(void);
#endif

#if CONFIG_OPENTHREAD_SPINEL_ONLY
/**
 * @brief Перевірка версії прошивки RCP та оновлення за потреби
 */
esp_err_t check_ot_rcp_version(void);
#endif

#ifdef __cplusplus
}
#endif
