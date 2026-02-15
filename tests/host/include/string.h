#pragma once

#include <stddef.h>
#include_next <string.h>

size_t strlcpy(char *dst, const char *src, size_t dst_size);
