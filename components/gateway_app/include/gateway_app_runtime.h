#pragma once

#include "api_usecases.h"
#include "device_service.h"
#include "esp_err.h"
#include "gateway_jobs_facade.h"
#include "gateway_wifi_system_facade.h"
#include "state_store.h"
#include "ws_manager.h"
#include "zigbee_service.h"

typedef struct {
    device_service_handle_t device_service;
    gateway_state_handle_t gateway_state;
    zigbee_service_handle_t zigbee_service;
    gateway_wifi_system_handle_t wifi_system;
    gateway_jobs_handle_t jobs;
    api_usecases_handle_t api_usecases;
    ws_manager_handle_t ws_manager;
} gateway_app_runtime_handles_t;

esp_err_t gateway_app_runtime_create(gateway_app_runtime_handles_t *out_handles);
void gateway_app_runtime_destroy(gateway_app_runtime_handles_t *handles);
