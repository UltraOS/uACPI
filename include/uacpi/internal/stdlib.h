#pragma once

#include <uacpi/internal/types.h>
#include <uacpi/platform/stdlib.h>

#define uacpi_memzero(ptr, size) uacpi_memset(ptr, 0, size)

#define UACPI_COMPARE(x, y, op) ((x) op (y) ? (x) : (y))
#define UACPI_MIN(x, y) UACPI_COMPARE(x, y, <)
#define UACPI_MAX(x, y) UACPI_COMPARE(x, y, >)

void uacpi_memcpy_zerout(void *dst, const void *src,
                         uacpi_size dst_size, uacpi_size src_size);
