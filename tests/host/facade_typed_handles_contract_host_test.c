#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "gateway_jobs_facade.h"
#include "gateway_wifi_system_facade.h"
#include "job_queue.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef esp_err_t (*sig_gateway_wifi_system_create_t)(const gateway_wifi_system_init_params_t *,
                                                      gateway_wifi_system_handle_t *);
typedef esp_err_t (*sig_gateway_jobs_create_t)(const gateway_jobs_init_params_t *, gateway_jobs_handle_t *);
typedef esp_err_t (*sig_gateway_jobs_set_zigbee_service_t)(gateway_jobs_handle_t, struct zigbee_service *);
typedef esp_err_t (*sig_job_queue_set_zigbee_service_t)(job_queue_handle_t, zigbee_service_handle_t);

ASSERT_SIG(gateway_wifi_system_create, sig_gateway_wifi_system_create_t);
ASSERT_SIG(gateway_jobs_create, sig_gateway_jobs_create_t);
ASSERT_SIG(gateway_jobs_set_zigbee_service, sig_gateway_jobs_set_zigbee_service_t);
ASSERT_SIG(job_queue_set_zigbee_service_with_handle, sig_job_queue_set_zigbee_service_t);

_Static_assert(__builtin_types_compatible_p(
                   __typeof__(((gateway_wifi_system_init_params_t *)0)->gateway_state_handle),
                   struct gateway_state_store *),
               "gateway_wifi_system_init_params_t.gateway_state_handle must be typed");
_Static_assert(__builtin_types_compatible_p(
                   __typeof__(((gateway_jobs_init_params_t *)0)->job_queue_handle),
                   struct zgw_job_queue *),
               "gateway_jobs_init_params_t.job_queue_handle must be typed");

int main(void)
{
    printf("Running host tests: facade_typed_handles_contract_host_test\n");

    gateway_wifi_system_init_params_t wifi_params = {0};
    gateway_jobs_init_params_t jobs_params = {0};
    assert(wifi_params.gateway_state_handle == NULL);
    assert(jobs_params.job_queue_handle == NULL);

    printf("Host tests passed: facade_typed_handles_contract_host_test\n");
    return 0;
}
