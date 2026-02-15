#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "device_service_rules.h"

static void set_ieee(gateway_ieee_addr_t out, uint8_t seed)
{
    for (size_t i = 0; i < sizeof(gateway_ieee_addr_t); i++) {
        out[i] = (uint8_t)(seed + i);
    }
}

static void test_upsert_add_and_update(void)
{
    gateway_device_record_t devices[2] = {0};
    int count = 0;
    gateway_ieee_addr_t ieee_a = {0};
    gateway_ieee_addr_t ieee_b = {0};
    set_ieee(ieee_a, 0x10);
    set_ieee(ieee_b, 0x20);

    device_service_rules_result_t add_result = device_service_rules_upsert(
        devices, &count, 2, 0x1234, ieee_a, "Device");
    assert(add_result == DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(count == 1);
    assert(devices[0].short_addr == 0x1234);
    assert(memcmp(devices[0].ieee_addr, ieee_a, sizeof(gateway_ieee_addr_t)) == 0);
    assert(strcmp(devices[0].name, "Device 0x1234") == 0);

    device_service_rules_result_t update_result = device_service_rules_upsert(
        devices, &count, 2, 0x1234, ieee_b, "Device");
    assert(update_result == DEVICE_SERVICE_RULES_RESULT_UPDATED);
    assert(count == 1);
    assert(memcmp(devices[0].ieee_addr, ieee_b, sizeof(gateway_ieee_addr_t)) == 0);
}

static void test_upsert_limit_and_fallback_prefix(void)
{
    gateway_device_record_t devices[1] = {0};
    int count = 0;
    gateway_ieee_addr_t ieee_a = {0};
    gateway_ieee_addr_t ieee_b = {0};
    set_ieee(ieee_a, 0x30);
    set_ieee(ieee_b, 0x40);

    assert(device_service_rules_upsert(devices, &count, 1, 0x0001, ieee_a, NULL) ==
           DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(strcmp(devices[0].name, "Device 0x0001") == 0);

    assert(device_service_rules_upsert(devices, &count, 1, 0x0002, ieee_b, NULL) ==
           DEVICE_SERVICE_RULES_RESULT_LIMIT_REACHED);
    assert(count == 1);
}

static void test_rename_and_find(void)
{
    gateway_device_record_t devices[2] = {0};
    int count = 0;
    gateway_ieee_addr_t ieee = {0};
    set_ieee(ieee, 0x50);

    assert(device_service_rules_upsert(devices, &count, 2, 0x1111, ieee, "Device") ==
           DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(device_service_rules_find_index_by_short_addr(devices, count, 0x1111) == 0);
    assert(device_service_rules_find_index_by_short_addr(devices, count, 0x2222) == -1);

    assert(device_service_rules_rename(devices, count, 0x1111, "Kitchen Sensor"));
    assert(strcmp(devices[0].name, "Kitchen Sensor") == 0);
    assert(!device_service_rules_rename(devices, count, 0x2222, "Nope"));
}

static void test_delete_compacts_array(void)
{
    gateway_device_record_t devices[3] = {0};
    gateway_device_record_t deleted = {0};
    int count = 0;
    gateway_ieee_addr_t ieee_a = {0};
    gateway_ieee_addr_t ieee_b = {0};
    gateway_ieee_addr_t ieee_c = {0};
    set_ieee(ieee_a, 0x61);
    set_ieee(ieee_b, 0x71);
    set_ieee(ieee_c, 0x81);

    assert(device_service_rules_upsert(devices, &count, 3, 0x0101, ieee_a, "Device") ==
           DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(device_service_rules_upsert(devices, &count, 3, 0x0202, ieee_b, "Device") ==
           DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(device_service_rules_upsert(devices, &count, 3, 0x0303, ieee_c, "Device") ==
           DEVICE_SERVICE_RULES_RESULT_ADDED);
    assert(count == 3);

    assert(device_service_rules_delete_by_short_addr(devices, &count, 0x0202, &deleted));
    assert(count == 2);
    assert(deleted.short_addr == 0x0202);
    assert(devices[0].short_addr == 0x0101);
    assert(devices[1].short_addr == 0x0303);
    assert(!device_service_rules_delete_by_short_addr(devices, &count, 0x9999, &deleted));
}

int main(void)
{
    printf("Running host tests: device_service_rules_host_test\n");
    test_upsert_add_and_update();
    test_upsert_limit_and_fallback_prefix();
    test_rename_and_find();
    test_delete_compacts_array();
    printf("Host tests passed: device_service_rules_host_test\n");
    return 0;
}
