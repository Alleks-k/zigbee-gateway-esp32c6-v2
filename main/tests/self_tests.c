#include "self_tests.h"

#include "unity.h"
#include "esp_log.h"

static const char *TAG = "SELF_TESTS";

/* Component test suites (defined in gateway_core/test and gateway_web/test). */
extern void gateway_core_register_self_tests(void);
extern void gateway_web_register_self_tests(void);

/* E2E/integration tests kept in main/tests. */
extern void zgw_register_e2e_self_tests(void);

int zgw_run_self_tests(void)
{
    ESP_LOGW(TAG, "Running gateway self-tests");
    UNITY_BEGIN();
    gateway_core_register_self_tests();
    gateway_web_register_self_tests();
    zgw_register_e2e_self_tests();
    int failures = UNITY_END();
    ESP_LOGW(TAG, "Self-tests complete, failures=%d", failures);
    return failures;
}
