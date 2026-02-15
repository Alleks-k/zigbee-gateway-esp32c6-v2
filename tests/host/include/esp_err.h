#pragma once

#include <stdint.h>

typedef int32_t esp_err_t;

#define ESP_OK 0
#define ESP_FAIL 0x0001
#define ESP_ERR_NO_MEM 0x0101
#define ESP_ERR_INVALID_ARG 0x0102
#define ESP_ERR_INVALID_SIZE 0x0103
#define ESP_ERR_NOT_FOUND 0x0104
#define ESP_ERR_NOT_SUPPORTED 0x0105
#define ESP_ERR_INVALID_VERSION 0x0106

static inline const char *esp_err_to_name(esp_err_t err)
{
    switch (err) {
    case ESP_OK:
        return "ESP_OK";
    case ESP_FAIL:
        return "ESP_FAIL";
    case ESP_ERR_NO_MEM:
        return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG:
        return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_SIZE:
        return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND:
        return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_NOT_SUPPORTED:
        return "ESP_ERR_NOT_SUPPORTED";
    case ESP_ERR_INVALID_VERSION:
        return "ESP_ERR_INVALID_VERSION";
    default:
        return "ESP_ERR_UNKNOWN";
    }
}
