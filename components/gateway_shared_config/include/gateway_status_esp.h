#pragma once

#include "esp_err.h"
#include "gateway_status.h"

static inline esp_err_t gateway_status_to_esp_err(gateway_status_t status)
{
    switch (status) {
    case GATEWAY_STATUS_OK:
        return ESP_OK;
    case GATEWAY_STATUS_INVALID_ARG:
        return ESP_ERR_INVALID_ARG;
    case GATEWAY_STATUS_NO_MEM:
        return ESP_ERR_NO_MEM;
    case GATEWAY_STATUS_NOT_FOUND:
        return ESP_ERR_NOT_FOUND;
    case GATEWAY_STATUS_NOT_SUPPORTED:
        return ESP_ERR_NOT_SUPPORTED;
    case GATEWAY_STATUS_INVALID_STATE:
        return ESP_ERR_INVALID_STATE;
    case GATEWAY_STATUS_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case GATEWAY_STATUS_FAIL:
    default:
        return ESP_FAIL;
    }
}

static inline gateway_status_t gateway_status_from_esp_err(esp_err_t err)
{
    switch (err) {
    case ESP_OK:
        return GATEWAY_STATUS_OK;
    case ESP_ERR_INVALID_ARG:
        return GATEWAY_STATUS_INVALID_ARG;
    case ESP_ERR_NO_MEM:
        return GATEWAY_STATUS_NO_MEM;
    case ESP_ERR_NOT_FOUND:
        return GATEWAY_STATUS_NOT_FOUND;
    case ESP_ERR_NOT_SUPPORTED:
        return GATEWAY_STATUS_NOT_SUPPORTED;
    case ESP_ERR_INVALID_STATE:
        return GATEWAY_STATUS_INVALID_STATE;
    case ESP_ERR_TIMEOUT:
        return GATEWAY_STATUS_TIMEOUT;
    default:
        return GATEWAY_STATUS_FAIL;
    }
}
