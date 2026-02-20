#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "gateway_runtime_context.h"
#include "gateway_zigbee_runtime.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef esp_err_t (*sig_gateway_zigbee_runtime_create_t)(const gateway_runtime_context_t *, gateway_zigbee_runtime_handle_t *);
typedef void (*sig_gateway_zigbee_runtime_destroy_t)(gateway_zigbee_runtime_handle_t);
typedef esp_err_t (*sig_gateway_zigbee_runtime_start_t)(gateway_zigbee_runtime_handle_t);
typedef zigbee_service_handle_t (*sig_gateway_zigbee_runtime_get_service_handle_t)(gateway_zigbee_runtime_handle_t);

ASSERT_SIG(gateway_zigbee_runtime_create, sig_gateway_zigbee_runtime_create_t);
ASSERT_SIG(gateway_zigbee_runtime_destroy, sig_gateway_zigbee_runtime_destroy_t);
ASSERT_SIG(gateway_zigbee_runtime_start, sig_gateway_zigbee_runtime_start_t);
ASSERT_SIG(gateway_zigbee_runtime_get_service_handle, sig_gateway_zigbee_runtime_get_service_handle_t);

int main(void)
{
    printf("Running host tests: gateway_zigbee_runtime_ctx_host_test\n");

    gateway_runtime_context_t ctx = {
        .device_service = NULL,
        .gateway_state = NULL,
    };
    gateway_zigbee_runtime_handle_t handle = NULL;

    assert(ctx.device_service == NULL);
    assert(ctx.gateway_state == NULL);
    assert(handle == NULL);

    printf("Host tests passed: gateway_zigbee_runtime_ctx_host_test\n");
    return 0;
}
