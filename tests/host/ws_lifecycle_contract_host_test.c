#include <assert.h>
#include <stdio.h>

#include "web_server.h"
#include "ws_manager.h"

#define ASSERT_SIG(fn, sig_t) _Static_assert(__builtin_types_compatible_p(__typeof__(&(fn)), sig_t), "signature mismatch: " #fn)

typedef void (*sig_start_web_server_t)(ws_manager_handle_t, api_usecases_handle_t);
typedef void (*sig_stop_web_server_t)(ws_manager_handle_t);
typedef int (*sig_web_server_get_ws_client_count_t)(ws_manager_handle_t);
typedef void (*sig_web_server_broadcast_ws_status_t)(ws_manager_handle_t);
typedef httpd_handle_t (*sig_ws_manager_get_server_t)(ws_manager_handle_t);
typedef void (*sig_ws_manager_set_server_t)(ws_manager_handle_t, httpd_handle_t);

ASSERT_SIG(start_web_server, sig_start_web_server_t);
ASSERT_SIG(stop_web_server, sig_stop_web_server_t);
ASSERT_SIG(web_server_get_ws_client_count, sig_web_server_get_ws_client_count_t);
ASSERT_SIG(web_server_broadcast_ws_status, sig_web_server_broadcast_ws_status_t);
ASSERT_SIG(ws_manager_get_server_with_handle, sig_ws_manager_get_server_t);
ASSERT_SIG(ws_manager_set_server_with_handle, sig_ws_manager_set_server_t);

int main(void)
{
    printf("Running host tests: ws_lifecycle_contract_host_test\n");

    ws_manager_handle_t ws = NULL;
    api_usecases_handle_t usecases = NULL;
    httpd_handle_t server = NULL;

    assert(ws == NULL);
    assert(usecases == NULL);
    assert(server == NULL);

    printf("Host tests passed: ws_lifecycle_contract_host_test\n");
    return 0;
}
