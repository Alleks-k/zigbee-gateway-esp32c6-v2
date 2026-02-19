#pragma once

#include <stdbool.h>
#include "esp_http_server.h"
#include "ws_manager.h"

bool http_routes_register(httpd_handle_t server, ws_manager_handle_t ws_manager);
