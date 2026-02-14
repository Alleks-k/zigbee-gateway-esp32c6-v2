#pragma once

#include <stdint.h>

// Project-level canonical 64-bit IEEE address type to avoid leaking Zigbee SDK types in public headers.
typedef uint8_t gateway_ieee_addr_t[8];
