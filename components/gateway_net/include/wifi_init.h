#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "wifi_context.h"

/**
 * @brief Initialize Wi-Fi STA and wait until GOT_IP
 *
 * @return esp_err_t
 */
esp_err_t wifi_init_sta_and_wait(void);
void wifi_state_store_update(void);

#if CONFIG_GATEWAY_SELF_TEST_APP
typedef struct {
    void (*net_platform_services_init)(void);
    esp_err_t (*wifi_sta_connect_and_wait)(wifi_runtime_ctx_t *ctx);
    esp_err_t (*wifi_start_fallback_ap)(wifi_runtime_ctx_t *ctx);
} wifi_init_ops_t;

void wifi_init_set_ops_for_test(const wifi_init_ops_t *ops);
void wifi_init_reset_ops_for_test(void);
#endif
