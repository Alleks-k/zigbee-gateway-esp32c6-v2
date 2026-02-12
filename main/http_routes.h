#pragma once

#include <stdbool.h>
#include "esp_http_server.h"

bool http_routes_register(httpd_handle_t server);
