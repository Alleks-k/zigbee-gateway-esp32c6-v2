#include "gateway_persistence_adapter.h"

#include "config_repository.h"
#include "device_repository.h"
#include "gateway_status_esp.h"
#include "storage_partitions.h"
#include "storage_schema.h"

gateway_status_t gateway_persistence_schema_init(void)
{
    return gateway_status_from_esp_err(storage_schema_init());
}

gateway_status_t gateway_persistence_schema_get_version(int32_t *out_version, bool *out_found)
{
    return gateway_status_from_esp_err(storage_schema_get_version(out_version, out_found));
}

gateway_status_t gateway_persistence_schema_set_version(int32_t version)
{
    return gateway_status_from_esp_err(storage_schema_set_version(version));
}

gateway_status_t gateway_persistence_config_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                                  char *password, size_t password_size,
                                                                  bool *loaded)
{
    return gateway_status_from_esp_err(
        config_repository_load_wifi_credentials(ssid, ssid_size, password, password_size, loaded));
}

gateway_status_t gateway_persistence_config_save_wifi_credentials(const char *ssid, const char *password)
{
    return gateway_status_from_esp_err(config_repository_save_wifi_credentials(ssid, password));
}

gateway_status_t gateway_persistence_config_clear_wifi_credentials(void)
{
    return gateway_status_from_esp_err(config_repository_clear_wifi_credentials());
}

gateway_status_t gateway_persistence_devices_load(gateway_device_record_t *devices, size_t max_devices,
                                                  int *device_count, bool *loaded)
{
    return gateway_status_from_esp_err(device_repository_load(devices, max_devices, device_count, loaded));
}

gateway_status_t gateway_persistence_devices_save(const gateway_device_record_t *devices, size_t max_devices,
                                                  int device_count)
{
    return gateway_status_from_esp_err(device_repository_save(devices, max_devices, device_count));
}

gateway_status_t gateway_persistence_devices_clear(void)
{
    return gateway_status_from_esp_err(device_repository_clear());
}

gateway_status_t gateway_persistence_partitions_erase_zigbee_storage(void)
{
    return gateway_status_from_esp_err(storage_partitions_erase_zigbee_storage());
}

gateway_status_t gateway_persistence_partitions_erase_zigbee_factory(void)
{
    return gateway_status_from_esp_err(storage_partitions_erase_zigbee_factory());
}
