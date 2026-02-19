#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gateway_config_types.h"
#include "gateway_status.h"

gateway_status_t gateway_persistence_schema_init(void);
gateway_status_t gateway_persistence_schema_get_version(int32_t *out_version, bool *out_found);
gateway_status_t gateway_persistence_schema_set_version(int32_t version);

gateway_status_t gateway_persistence_config_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                                  char *password, size_t password_size,
                                                                  bool *loaded);
gateway_status_t gateway_persistence_config_save_wifi_credentials(const char *ssid, const char *password);
gateway_status_t gateway_persistence_config_clear_wifi_credentials(void);

gateway_status_t gateway_persistence_devices_load(gateway_device_record_t *devices, size_t max_devices,
                                                  int *device_count, bool *loaded);
gateway_status_t gateway_persistence_devices_save(const gateway_device_record_t *devices, size_t max_devices,
                                                  int device_count);
gateway_status_t gateway_persistence_devices_clear(void);

gateway_status_t gateway_persistence_partitions_erase_zigbee_storage(void);
gateway_status_t gateway_persistence_partitions_erase_zigbee_factory(void);
