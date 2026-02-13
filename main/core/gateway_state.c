#include "gateway_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static SemaphoreHandle_t s_state_mutex = NULL;
static gateway_network_state_t s_network_state = {0};

esp_err_t gateway_state_init(void)
{
    if (s_state_mutex == NULL) {
        s_state_mutex = xSemaphoreCreateMutex();
        if (s_state_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t gateway_state_set_network(const gateway_network_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_network_state = *state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}

esp_err_t gateway_state_get_network(gateway_network_state_t *out_state)
{
    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = gateway_state_init();
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    *out_state = s_network_state;
    xSemaphoreGive(s_state_mutex);
    return ESP_OK;
}
