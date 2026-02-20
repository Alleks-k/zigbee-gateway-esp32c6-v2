#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "state_store.h"
#include "wifi_context.h"

/**
 * @brief Initialize Wi-Fi STA and wait until GOT_IP
 *
 * @return esp_err_t
 */
esp_err_t wifi_init_sta_and_wait(wifi_runtime_ctx_t *ctx);
void wifi_state_store_update(wifi_runtime_ctx_t *ctx);
esp_err_t wifi_init_bind_state(wifi_runtime_ctx_t *ctx,
                               gateway_state_handle_t state_handle,
                               struct wifi_service *wifi_service,
                               struct system_service *system_service);

#if CONFIG_GATEWAY_SELF_TEST_APP
void wifi_init_set_ops_for_test(wifi_runtime_ctx_t *ctx, const wifi_init_ops_t *ops);
void wifi_init_reset_ops_for_test(wifi_runtime_ctx_t *ctx);
#endif
