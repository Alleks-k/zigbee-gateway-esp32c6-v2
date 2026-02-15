#include "storage_partitions.h"

#include "storage_kv.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_partitions_mutex = NULL;

static esp_err_t partitions_lock(void)
{
    if (s_partitions_mutex == NULL) {
        s_partitions_mutex = xSemaphoreCreateMutex();
        if (s_partitions_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreTake(s_partitions_mutex, portMAX_DELAY);
    return ESP_OK;
}

static void partitions_unlock(void)
{
    if (s_partitions_mutex != NULL) {
        xSemaphoreGive(s_partitions_mutex);
    }
}

esp_err_t storage_partitions_erase_zigbee_storage(void)
{
    esp_err_t err = partitions_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = storage_kv_erase_partition("zb_storage", NULL);
    partitions_unlock();
    return err;
}

esp_err_t storage_partitions_erase_zigbee_factory(void)
{
    esp_err_t err = partitions_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = storage_kv_erase_partition("zb_fct", NULL);
    partitions_unlock();
    return err;
}
