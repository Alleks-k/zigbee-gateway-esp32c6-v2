#pragma once

struct device_service;
struct gateway_state_store;

typedef struct {
    struct device_service *device_service;
    struct gateway_state_store *gateway_state;
} gateway_runtime_context_t;

