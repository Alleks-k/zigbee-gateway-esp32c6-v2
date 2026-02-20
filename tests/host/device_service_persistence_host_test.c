#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_service.h"

typedef struct {
    gateway_status_t load_status;
    bool load_loaded;
    gateway_device_record_t load_devices[MAX_DEVICES];
    int load_count;

    gateway_status_t save_status;
    int save_calls;
} repo_stub_t;

typedef struct {
    int list_changed_calls;
    int delete_request_calls;
    uint16_t last_deleted_short_addr;
    gateway_ieee_addr_t last_deleted_ieee;
} notifier_stub_t;

static repo_stub_t g_repo;
static notifier_stub_t g_notifier;

static void reset_stubs(void)
{
    memset(&g_repo, 0, sizeof(g_repo));
    memset(&g_notifier, 0, sizeof(g_notifier));
    g_repo.load_status = GATEWAY_STATUS_OK;
    g_repo.save_status = GATEWAY_STATUS_OK;
}

static gateway_status_t lock_create(void *ctx, void **out_lock)
{
    (void)ctx;
    if (!out_lock) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    *out_lock = (void *)0x1;
    return GATEWAY_STATUS_OK;
}

static void lock_destroy(void *ctx, void *lock)
{
    (void)ctx;
    (void)lock;
}

static void lock_enter(void *ctx, void *lock)
{
    (void)ctx;
    (void)lock;
}

static void lock_exit(void *ctx, void *lock)
{
    (void)ctx;
    (void)lock;
}

static gateway_status_t repo_load(void *ctx,
                                  gateway_device_record_t *devices,
                                  size_t max_devices,
                                  int *device_count,
                                  bool *loaded)
{
    (void)ctx;
    if (!devices || !device_count || !loaded) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (g_repo.load_status != GATEWAY_STATUS_OK) {
        return g_repo.load_status;
    }

    *loaded = g_repo.load_loaded;
    if (!g_repo.load_loaded) {
        *device_count = 0;
        return GATEWAY_STATUS_OK;
    }

    if (g_repo.load_count < 0 || (size_t)g_repo.load_count > max_devices) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    memcpy(devices, g_repo.load_devices, sizeof(gateway_device_record_t) * (size_t)g_repo.load_count);
    *device_count = g_repo.load_count;
    return GATEWAY_STATUS_OK;
}

static gateway_status_t repo_save(void *ctx,
                                  const gateway_device_record_t *devices,
                                  size_t max_devices,
                                  int device_count)
{
    (void)ctx;
    (void)devices;
    (void)max_devices;
    (void)device_count;
    g_repo.save_calls++;
    return g_repo.save_status;
}

static void on_list_changed(void *ctx)
{
    (void)ctx;
    g_notifier.list_changed_calls++;
}

static void on_delete_request(void *ctx, uint16_t short_addr, const gateway_ieee_addr_t ieee_addr)
{
    (void)ctx;
    g_notifier.delete_request_calls++;
    g_notifier.last_deleted_short_addr = short_addr;
    memcpy(g_notifier.last_deleted_ieee, ieee_addr, sizeof(g_notifier.last_deleted_ieee));
}

static void set_ieee(gateway_ieee_addr_t out, uint8_t seed)
{
    for (size_t i = 0; i < sizeof(gateway_ieee_addr_t); i++) {
        out[i] = (uint8_t)(seed + i);
    }
}

static device_service_handle_t make_service(void)
{
    static const device_service_lock_port_t lock_port = {
        .create = lock_create,
        .destroy = lock_destroy,
        .enter = lock_enter,
        .exit = lock_exit,
        .ctx = NULL,
    };
    static const device_service_repo_port_t repo_port = {
        .load = repo_load,
        .save = repo_save,
        .ctx = NULL,
    };
    static const device_service_notifier_t notifier = {
        .on_list_changed = on_list_changed,
        .on_delete_request = on_delete_request,
        .ctx = NULL,
    };
    device_service_init_params_t params = {
        .lock_port = &lock_port,
        .repo_port = &repo_port,
        .notifier = &notifier,
    };

    device_service_handle_t handle = NULL;
    assert(device_service_create_with_params(&params, &handle) == GATEWAY_STATUS_OK);
    return handle;
}

static void test_init_propagates_load_error(void)
{
    reset_stubs();
    g_repo.load_status = GATEWAY_STATUS_FAIL;

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_FAIL);

    gateway_device_record_t snapshot[MAX_DEVICES] = {0};
    assert(device_service_get_snapshot(handle, snapshot, MAX_DEVICES) == 0);
    device_service_destroy(handle);
}

static void test_add_rolls_back_on_save_failure(void)
{
    reset_stubs();
    g_repo.save_status = GATEWAY_STATUS_FAIL;

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_OK);

    gateway_ieee_addr_t ieee = {0};
    set_ieee(ieee, 0x10);
    assert(device_service_add_with_ieee(handle, 0x1234, ieee) == GATEWAY_STATUS_FAIL);
    assert(g_repo.save_calls == 1);
    assert(g_notifier.list_changed_calls == 0);

    gateway_device_record_t snapshot[MAX_DEVICES] = {0};
    assert(device_service_get_snapshot(handle, snapshot, MAX_DEVICES) == 0);
    device_service_destroy(handle);
}

static void test_update_same_name_is_noop(void)
{
    reset_stubs();

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_OK);

    gateway_ieee_addr_t ieee = {0};
    set_ieee(ieee, 0x20);
    assert(device_service_add_with_ieee(handle, 0x4321, ieee) == GATEWAY_STATUS_OK);
    assert(g_notifier.list_changed_calls == 1);
    assert(g_repo.save_calls == 1);

    gateway_device_record_t snapshot[MAX_DEVICES] = {0};
    int count = device_service_get_snapshot(handle, snapshot, MAX_DEVICES);
    assert(count == 1);

    g_notifier.list_changed_calls = 0;
    g_repo.save_calls = 0;
    assert(device_service_update_name(handle, 0x4321, snapshot[0].name) == GATEWAY_STATUS_OK);
    assert(g_repo.save_calls == 0);
    assert(g_notifier.list_changed_calls == 0);

    device_service_destroy(handle);
}

static void test_delete_rolls_back_on_save_failure(void)
{
    reset_stubs();

    device_service_handle_t handle = make_service();
    assert(device_service_init(handle) == GATEWAY_STATUS_OK);

    gateway_ieee_addr_t ieee = {0};
    set_ieee(ieee, 0x30);
    assert(device_service_add_with_ieee(handle, 0x1111, ieee) == GATEWAY_STATUS_OK);

    g_repo.save_status = GATEWAY_STATUS_FAIL;
    g_repo.save_calls = 0;
    g_notifier.list_changed_calls = 0;
    g_notifier.delete_request_calls = 0;
    assert(device_service_delete(handle, 0x1111) == GATEWAY_STATUS_FAIL);
    assert(g_repo.save_calls == 1);
    assert(g_notifier.delete_request_calls == 0);
    assert(g_notifier.list_changed_calls == 0);

    gateway_device_record_t snapshot[MAX_DEVICES] = {0};
    assert(device_service_get_snapshot(handle, snapshot, MAX_DEVICES) == 1);

    device_service_destroy(handle);
}

int main(void)
{
    printf("Running host tests: device_service_persistence_host_test\n");
    test_init_propagates_load_error();
    test_add_rolls_back_on_save_failure();
    test_update_same_name_is_noop();
    test_delete_rolls_back_on_save_failure();
    printf("Host tests passed: device_service_persistence_host_test\n");
    return 0;
}
