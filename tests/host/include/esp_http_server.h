#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void *httpd_handle_t;

typedef struct httpd_req {
    int method;
    const char *uri;
    void *user_ctx;
} httpd_req_t;

typedef struct {
    uint8_t type;
    uint8_t *payload;
    size_t len;
} httpd_ws_frame_t;
