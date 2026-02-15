#include "unity.h"
#include "storage_kv.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define STORAGE_TEST_NS "zgw_tst"

static void test_storage_kv_open_invalid_args(void)
{
    storage_kv_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_kv_open_readwrite(NULL, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_kv_open_readwrite(STORAGE_TEST_NS, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_kv_open_readonly(NULL, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_kv_open_readonly(STORAGE_TEST_NS, NULL));
}

static void test_storage_kv_u32_roundtrip(void)
{
    storage_kv_handle_t write_handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_open_readwrite(STORAGE_TEST_NS, &write_handle));

    bool existed = false;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_erase_key(write_handle, "u32v", &existed));
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_set_u32(write_handle, "u32v", 0xA5A55A5Au));
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_commit(write_handle));
    storage_kv_close(write_handle);

    storage_kv_handle_t read_handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_open_readonly(STORAGE_TEST_NS, &read_handle));
    uint32_t value = 0;
    bool found = false;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_get_u32(read_handle, "u32v", &value, &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_HEX32(0xA5A55A5Au, value);
    storage_kv_close(read_handle);
}

static void test_storage_kv_blob_roundtrip_and_erase(void)
{
    const uint8_t in_blob[] = {0x10, 0x20, 0x30, 0x40, 0xAA, 0x55};
    uint8_t out_blob[sizeof(in_blob)] = {0};
    size_t out_len = 0;

    storage_kv_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_open_readwrite(STORAGE_TEST_NS, &handle));

    bool existed = false;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_erase_key(handle, "blobv", &existed));
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_set_blob(handle, "blobv", in_blob, sizeof(in_blob)));
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_commit(handle));

    bool found = false;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_get_blob(handle, "blobv", out_blob, sizeof(out_blob), &out_len, &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT(sizeof(in_blob), out_len);
    TEST_ASSERT_EQUAL_MEMORY(in_blob, out_blob, sizeof(in_blob));

    existed = false;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_erase_key(handle, "blobv", &existed));
    TEST_ASSERT_TRUE(existed);
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_commit(handle));

    memset(out_blob, 0, sizeof(out_blob));
    out_len = 123;
    found = true;
    TEST_ASSERT_EQUAL(ESP_OK, storage_kv_get_blob(handle, "blobv", out_blob, sizeof(out_blob), &out_len, &found));
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL_UINT(0, out_len);

    storage_kv_close(handle);
}

void gateway_core_storage_register_self_tests(void)
{
    RUN_TEST(test_storage_kv_open_invalid_args);
    RUN_TEST(test_storage_kv_u32_roundtrip);
    RUN_TEST(test_storage_kv_blob_roundtrip_and_erase);
}
