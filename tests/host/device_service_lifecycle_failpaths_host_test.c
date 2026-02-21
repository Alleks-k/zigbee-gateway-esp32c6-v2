#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_service.h"

typedef struct {
    gateway_status_t create_ret;
    int create_calls;
    int destroy_calls;
    int enter_calls;
    int exit_calls;
    void *created_lock;
} lock_stub_t;

typedef struct {
    gateway_status_t load_ret;
    bool load_loaded;
    int load_count;
    int load_calls;
    int save_calls;
} repo_stub_t;

static lock_stub_t g_lock_stub;
static repo_stub_t g_repo_stub;

static void reset_lock_stub(void)
{
    memset(&g_lock_stub, 0, sizeof(g_lock_stub));
    g_lock_stub.create_ret = GATEWAY_STATUS_OK;
    g_lock_stub.created_lock = (void *)(uintptr_t)0xCAFE;
}

static void reset_repo_stub(void)
{
    memset(&g_repo_stub, 0, sizeof(g_repo_stub));
    g_repo_stub.load_ret = GATEWAY_STATUS_OK;
}

static gateway_status_t lock_create(void *ctx, void **out_lock)
{
    lock_stub_t *stub = ctx ? (lock_stub_t *)ctx : &g_lock_stub;
    stub->create_calls++;
    if (!out_lock) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (stub->create_ret != GATEWAY_STATUS_OK) {
        return stub->create_ret;
    }
    *out_lock = stub->created_lock;
    return GATEWAY_STATUS_OK;
}

static void lock_destroy(void *ctx, void *lock)
{
    lock_stub_t *stub = ctx ? (lock_stub_t *)ctx : &g_lock_stub;
    (void)lock;
    stub->destroy_calls++;
}

static void lock_enter(void *ctx, void *lock)
{
    lock_stub_t *stub = ctx ? (lock_stub_t *)ctx : &g_lock_stub;
    (void)lock;
    stub->enter_calls++;
}

static void lock_exit(void *ctx, void *lock)
{
    lock_stub_t *stub = ctx ? (lock_stub_t *)ctx : &g_lock_stub;
    (void)lock;
    stub->exit_calls++;
}

static gateway_status_t repo_load(void *ctx,
                                  gateway_device_record_t *devices,
                                  size_t max_devices,
                                  int *device_count,
                                  bool *loaded)
{
    repo_stub_t *stub = ctx ? (repo_stub_t *)ctx : &g_repo_stub;
    stub->load_calls++;
    (void)devices;
    (void)max_devices;
    if (!device_count || !loaded) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (stub->load_ret != GATEWAY_STATUS_OK) {
        return stub->load_ret;
    }
    *loaded = stub->load_loaded;
    *device_count = stub->load_count;
    return GATEWAY_STATUS_OK;
}

static gateway_status_t repo_save(void *ctx,
                                  const gateway_device_record_t *devices,
                                  size_t max_devices,
                                  int device_count)
{
    repo_stub_t *stub = ctx ? (repo_stub_t *)ctx : &g_repo_stub;
    (void)devices;
    (void)max_devices;
    (void)device_count;
    stub->save_calls++;
    return GATEWAY_STATUS_OK;
}

static device_service_handle_t make_service(void)
{
    static device_service_lock_port_t lock_port = {
        .create = lock_create,
        .destroy = lock_destroy,
        .enter = lock_enter,
        .exit = lock_exit,
        .ctx = &g_lock_stub,
    };
    static device_service_repo_port_t repo_port = {
        .load = repo_load,
        .save = repo_save,
        .ctx = &g_repo_stub,
    };
    device_service_init_params_t params = {
        .lock_port = &lock_port,
        .repo_port = &repo_port,
        .notifier = NULL,
    };

    device_service_handle_t handle = NULL;
    assert(device_service_create_with_params(&params, &handle) == GATEWAY_STATUS_OK);
    return handle;
}

static void test_create_validates_arguments(void)
{
    assert(device_service_create_with_params(NULL, NULL) == GATEWAY_STATUS_INVALID_ARG);
    assert(device_service_create(NULL) == GATEWAY_STATUS_INVALID_ARG);
}

static void test_init_without_ports_fails_fast(void)
{
    device_service_handle_t handle = NULL;
    assert(device_service_create_with_params(NULL, &handle) == GATEWAY_STATUS_OK);
    assert(device_service_init(handle) == GATEWAY_STATUS_INVALID_STATE);
    device_service_destroy(handle);
}

static void test_init_lock_create_failure_short_circuits_repo_load(void)
{
    reset_lock_stub();
    reset_repo_stub();
    g_lock_stub.create_ret = GATEWAY_STATUS_NO_MEM;

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_NO_MEM);
    assert(g_lock_stub.create_calls == 1);
    assert(g_repo_stub.load_calls == 0);
    device_service_destroy(handle);
    assert(g_lock_stub.destroy_calls == 0);
}

static void test_init_repo_load_failure_keeps_destroy_safe(void)
{
    reset_lock_stub();
    reset_repo_stub();
    g_repo_stub.load_ret = GATEWAY_STATUS_FAIL;

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_FAIL);
    assert(g_lock_stub.create_calls == 1);
    assert(g_repo_stub.load_calls == 1);
    device_service_destroy(handle);
    assert(g_lock_stub.destroy_calls == 1);
}

static void test_init_is_lock_idempotent_and_destroy_releases_once(void)
{
    reset_lock_stub();
    reset_repo_stub();

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_OK);
    assert(device_service_init(handle) == GATEWAY_STATUS_OK);
    assert(g_lock_stub.create_calls == 1);
    assert(g_repo_stub.load_calls == 2);

    device_service_destroy(handle);
    assert(g_lock_stub.destroy_calls == 1);
}

int main(void)
{
    printf("Running host tests: device_service_lifecycle_failpaths_host_test\n");
    test_create_validates_arguments();
    test_init_without_ports_fails_fast();
    test_init_lock_create_failure_short_circuits_repo_load();
    test_init_repo_load_failure_keeps_destroy_safe();
    test_init_is_lock_idempotent_and_destroy_releases_once();
    printf("Host tests passed: device_service_lifecycle_failpaths_host_test\n");
    return 0;
}
