#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "api_usecases.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef esp_err_t (*sig_api_usecases_create_t)(const api_usecases_init_params_t *, api_usecases_handle_t *);
typedef void (*sig_api_usecases_destroy_t)(api_usecases_handle_t);
typedef void (*sig_api_usecases_set_runtime_handles_t)(api_usecases_handle_t, zigbee_service_handle_t,
                                                        gateway_wifi_system_handle_t, gateway_jobs_handle_t);
typedef void (*sig_api_usecases_set_ws_providers_t)(api_usecases_handle_t, api_ws_client_count_provider_t, api_ws_metrics_provider_t, void *);
typedef esp_err_t (*sig_api_usecase_collect_health_snapshot_t)(api_usecases_handle_t, api_health_snapshot_t *);

ASSERT_SIG(api_usecases_create, sig_api_usecases_create_t);
ASSERT_SIG(api_usecases_destroy, sig_api_usecases_destroy_t);
ASSERT_SIG(api_usecases_set_runtime_handles, sig_api_usecases_set_runtime_handles_t);
ASSERT_SIG(api_usecases_set_ws_providers, sig_api_usecases_set_ws_providers_t);
ASSERT_SIG(api_usecase_collect_health_snapshot, sig_api_usecase_collect_health_snapshot_t);

static uint32_t count_provider(void *ctx)
{
    return (ctx != NULL) ? *(const uint32_t *)ctx : 0u;
}

static bool metrics_provider(void *ctx, api_ws_runtime_metrics_t *out_metrics)
{
    if (!ctx || !out_metrics) {
        return false;
    }

    out_metrics->connections_total = *(const uint32_t *)ctx;
    out_metrics->reconnect_count = 1;
    out_metrics->dropped_frames_total = 2;
    out_metrics->broadcast_lock_skips_total = 3;
    return true;
}

int main(void)
{
    printf("Running host tests: api_usecases_ctx_host_test\n");

    uint32_t ws_count = 17;
    api_usecases_init_params_t params = {
        .service_ops = NULL,
        .wifi_system = NULL,
        .jobs = NULL,
        .ws_client_count_provider = count_provider,
        .ws_metrics_provider = metrics_provider,
        .ws_provider_ctx = &ws_count,
    };

    assert(params.ws_client_count_provider(params.ws_provider_ctx) == 17u);

    api_ws_runtime_metrics_t metrics = {0};
    assert(params.ws_metrics_provider(params.ws_provider_ctx, &metrics));
    assert(metrics.connections_total == 17u);
    assert(metrics.reconnect_count == 1u);
    assert(metrics.dropped_frames_total == 2u);
    assert(metrics.broadcast_lock_skips_total == 3u);

    printf("Host tests passed: api_usecases_ctx_host_test\n");
    return 0;
}
