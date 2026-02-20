#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "zigbee_service.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef esp_err_t (*sig_zigbee_service_create_t)(const zigbee_service_init_params_t *, zigbee_service_handle_t *);
typedef void (*sig_zigbee_service_destroy_t)(zigbee_service_handle_t);
typedef esp_err_t (*sig_zigbee_service_send_on_off_t)(zigbee_service_handle_t, uint16_t, uint8_t, uint8_t);
typedef esp_err_t (*sig_zigbee_service_get_network_status_t)(zigbee_service_handle_t, zigbee_network_status_t *);
typedef int (*sig_zigbee_service_get_devices_snapshot_t)(zigbee_service_handle_t, zb_device_t *, size_t);
typedef int (*sig_zigbee_service_get_neighbor_lqi_snapshot_t)(zigbee_service_handle_t, zigbee_neighbor_lqi_t *, size_t);

ASSERT_SIG(zigbee_service_create, sig_zigbee_service_create_t);
ASSERT_SIG(zigbee_service_destroy, sig_zigbee_service_destroy_t);
ASSERT_SIG(zigbee_service_send_on_off, sig_zigbee_service_send_on_off_t);
ASSERT_SIG(zigbee_service_get_network_status, sig_zigbee_service_get_network_status_t);
ASSERT_SIG(zigbee_service_get_devices_snapshot, sig_zigbee_service_get_devices_snapshot_t);
ASSERT_SIG(zigbee_service_get_neighbor_lqi_snapshot, sig_zigbee_service_get_neighbor_lqi_snapshot_t);

int main(void)
{
    printf("Running host tests: zigbee_service_ctx_host_test\n");

    zigbee_service_init_params_t params = {
        .device_service = NULL,
        .gateway_state = NULL,
        .runtime_ops = NULL,
    };

    assert(params.device_service == NULL);
    assert(params.gateway_state == NULL);
    assert(params.runtime_ops == NULL);

    printf("Host tests passed: zigbee_service_ctx_host_test\n");
    return 0;
}
