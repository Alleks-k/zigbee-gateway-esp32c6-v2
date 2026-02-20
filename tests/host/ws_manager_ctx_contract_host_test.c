#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "api_usecases.h"
#include "ws_manager.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef esp_err_t (*sig_ws_manager_create_t)(ws_manager_handle_t *);
typedef void (*sig_ws_manager_destroy_t)(ws_manager_handle_t);
typedef void (*sig_ws_manager_init_with_handle_t)(ws_manager_handle_t, httpd_handle_t, api_usecases_handle_t);
typedef esp_err_t (*sig_ws_handler_with_handle_t)(ws_manager_handle_t, httpd_req_t *);
typedef void (*sig_ws_broadcast_status_with_handle_t)(ws_manager_handle_t);

ASSERT_SIG(ws_manager_create, sig_ws_manager_create_t);
ASSERT_SIG(ws_manager_destroy, sig_ws_manager_destroy_t);
ASSERT_SIG(ws_manager_init_with_handle, sig_ws_manager_init_with_handle_t);
ASSERT_SIG(ws_handler_with_handle, sig_ws_handler_with_handle_t);
ASSERT_SIG(ws_broadcast_status_with_handle, sig_ws_broadcast_status_with_handle_t);

int main(void)
{
    printf("Running host tests: ws_manager_ctx_contract_host_test\n");

    ws_manager_handle_t ws_handle = NULL;
    httpd_handle_t server = NULL;
    api_usecases_handle_t usecases = NULL;

    assert(ws_handle == NULL);
    assert(server == NULL);
    assert(usecases == NULL);

    printf("Host tests passed: ws_manager_ctx_contract_host_test\n");
    return 0;
}
